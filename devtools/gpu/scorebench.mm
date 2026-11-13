// GPU feasibility: score W beam nodes against every algorithm (bit-sliced
// correct count, keep-solved reject, cost), appending (node, alg, score) when the
// score reaches a threshold.  Random data shaped like p8s10; measures pairs/s.
//   scorebench [W] [algs] [nodesPerThread]
#import <Metal/Metal.h>
#include <chrono>
#include <cstdio>
#include <random>
#include <vector>

static const char *src = R"(
#include <metal_stdlib>
using namespace metal;
struct Rec { ulong t0, t1, t2, x; int e4, pad0, pad1, pad2; };
struct NodeD { ulong c0, c1, c2, corr; int g4, pad0, pad1, pad2; };
struct Hit { uint node, alg; int score, pad; };
kernel void score(device const Rec *rec [[buffer(0)]], device const NodeD *nodes [[buffer(1)]],
                  device const int *tab [[buffer(2)]], device atomic_uint *count [[buffer(3)]],
                  device Hit *hits [[buffer(4)]], constant uint4 &P [[buffer(5)]],
                  uint2 gid [[thread_position_in_grid]]) {
  // P: x = algs, y = nodes, z = nodes per thread, w = max hits;  threshold in tab[63]
  const uint a = gid.x;
  if (a >= P.x) return;
  const Rec R = rec[a];
  const int thr = tab[63];
  const ulong M48 = (1ul << 48) - 1;
  const uint n0 = gid.y * P.z, n1 = min(n0 + P.z, P.y);
  for (uint n = n0; n < n1; n++) {
    const NodeD N = nodes[n];
    if (R.x & N.corr) continue;
    const int c = popcount(~((N.c0 ^ R.t0) | (N.c1 ^ R.t1) | (N.c2 ^ R.t2)) & M48);
    const int sc = tab[c] - N.g4 - R.e4;
    if (sc >= thr) {
      uint i = atomic_fetch_add_explicit(count, 1, memory_order_relaxed);
      if (i < P.w) hits[i] = Hit{n, a, sc, 0};
    }
  }
}
)";

struct Rec { uint64_t t0, t1, t2, x; int32_t e4, p0, p1, p2; };
struct NodeD { uint64_t c0, c1, c2, corr; int32_t g4, p0, p1, p2; };

int main(int argc, char **argv) {
  const uint32_t W = argc > 1 ? atoi(argv[1]) : 4096, A = argc > 2 ? atoi(argv[2]) : 407920, K = argc > 3 ? atoi(argv[3]) : 64;
  @autoreleasepool {
    id<MTLDevice> dev = MTLCreateSystemDefaultDevice();
    NSError *err = nil;
    id<MTLLibrary> lib = [dev newLibraryWithSource:[NSString stringWithUTF8String:src] options:nil error:&err];
    if (!lib) { printf("compile: %s\n", err.localizedDescription.UTF8String); return 1; }
    id<MTLComputePipelineState> ps = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"score"] error:&err];
    std::mt19937_64 rng(1);
    const uint64_t M48 = (1ULL << 48) - 1;
    id<MTLBuffer> bRec = [dev newBufferWithLength:sizeof(Rec) * A options:MTLResourceStorageModeShared];
    Rec *rec = (Rec *)bRec.contents;
    for (uint32_t a = 0; a < A; a++) {
      // an algorithm moving ~8 positions: target planes = identity colours except there
      rec[a] = {rng() & M48, rng() & M48, rng() & M48, rng() & rng() & rng() & M48, 28 + (int)(rng() % 5), 0, 0, 0};
    }
    id<MTLBuffer> bNodes = [dev newBufferWithLength:sizeof(NodeD) * W options:MTLResourceStorageModeShared];
    NodeD *nd = (NodeD *)bNodes.contents;
    for (uint32_t n = 0; n < W; n++) nd[n] = {rng() & M48, rng() & M48, rng() & M48, rng() & rng() & M48, (int)(rng() % 100), 0, 0, 0};
    id<MTLBuffer> bTab = [dev newBufferWithLength:64 * 4 options:MTLResourceStorageModeShared];
    int *tab = (int *)bTab.contents;
    for (int c = 0; c < 49; c++) tab[c] = 12 * c;
    tab[63] = 12 * 40 - 100;  // threshold: rare hits
    const uint32_t maxHits = 1 << 20;
    id<MTLBuffer> bCount = [dev newBufferWithLength:4 options:MTLResourceStorageModeShared];
    id<MTLBuffer> bHits = [dev newBufferWithLength:16 * maxHits options:MTLResourceStorageModeShared];
    uint32_t P[4] = {A, W, K, maxHits};
    id<MTLCommandQueue> q = [dev newCommandQueue];
    for (int rep = 0; rep < 4; rep++) {
      *(uint32_t *)bCount.contents = 0;
      auto t0 = std::chrono::steady_clock::now();
      id<MTLCommandBuffer> cb = [q commandBuffer];
      id<MTLComputeCommandEncoder> ce = [cb computeCommandEncoder];
      [ce setComputePipelineState:ps];
      [ce setBuffer:bRec offset:0 atIndex:0];
      [ce setBuffer:bNodes offset:0 atIndex:1];
      [ce setBuffer:bTab offset:0 atIndex:2];
      [ce setBuffer:bCount offset:0 atIndex:3];
      [ce setBuffer:bHits offset:0 atIndex:4];
      [ce setBytes:P length:sizeof P atIndex:5];
      MTLSize grid = MTLSizeMake(A, (W + K - 1) / K, 1);
      MTLSize tg = MTLSizeMake(ps.maxTotalThreadsPerThreadgroup, 1, 1);
      [ce dispatchThreads:grid threadsPerThreadgroup:tg];
      [ce endEncoding];
      [cb commit];
      [cb waitUntilCompleted];
      double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
      double gpu = cb.GPUEndTime - cb.GPUStartTime;
      printf("W %u x %u algs = %.2e pairs: wall %.1f ms, GPU %.1f ms -> %.2e pairs/s; hits %u\n", W, A, (double)W * A, s * 1e3, gpu * 1e3,
             (double)W * A / gpu, *(uint32_t *)bCount.contents);
    }
  }
}
