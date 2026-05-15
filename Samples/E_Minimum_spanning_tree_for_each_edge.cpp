#include "bits/stdc++.h"
#pragma GCC optimize("O3,unroll-loops")
using namespace std;
typedef long long ll;
const int B = 19;
struct UF {
  int n;
  vector<int> par, size;
  UF(int _n) : n(_n) {
    par.resize(n);
    size.resize(n, 1);
    for(int i = 0; i < n; i++) par[i] = i;
  }
  int find(int x) {
    if (par[x] == x) return x;
    return par[x] = find(par[x]);
  }
  void unite(int x, int y) {
    int a = find(x), b = find(y);
    if (a == b) return;
    if (size[a] < size[b]) swap(a, b);
    size[a] += size[b];
    par[b] = a;
  }
};
void solve() {
  int n, m; cin >> n >> m;
  vector<array<int, 4>> edges(m);
  for(int i = 0; i < m; i++) {
    int u, v, w; cin >> u >> v >> w;
    --u; --v;
    edges[i] = {w, u, v, i};
  }
  sort(edges.begin(), edges.end());
  vector<vector<pair<int, int>>> adj(n);
  UF uf(n);
  ll mst = 0;
  for(auto [w, u, v, i] : edges) {
    if (uf.find(u) != uf.find(v)) {
      mst += w;
      uf.unite(u, v);
      adj[u].push_back(make_pair(v, w));
      adj[v].push_back(make_pair(u, w));
    }
  }
  vector<int> dpth(n);
  vector<array<int, B>> up(n), mxup(n);
  for(int i = 0; i < n; i++) {
    up[i].fill(-1);
    mxup[i].fill(-1);
  }
  auto dfs = [&](auto &&self, int u, int p, int wp) -> void {
    up[u][0] = p;
    mxup[u][0] = wp;
    if (p != -1) {
      dpth[u] = dpth[p] + 1;
    }
    for(int i = 1; i < B; i++) {
      int pp = up[u][i - 1];
      if (pp != -1) {
        up[u][i] = up[pp][i - 1];
        mxup[u][i] = max(mxup[u][i - 1], mxup[pp][i - 1]);
      }
    }
    for(auto [v, w] : adj[u]) {
      if (v != p) {
        self(self, v, u, w);
      }
    }
  }; dfs(dfs, 0, -1, 0);
  vector<ll> answer(m);
  auto lca = [&](int u, int v) -> int {
    if (dpth[u] < dpth[v]) swap(u, v);
    for(int i = B - 1; i >= 0; i--) {
      if (dpth[u] - (1<<i) >= dpth[v]) {
        u = up[u][i];
      }
    }
    if (u == v) {
      return u;
    }
    for(int i = B - 1; i >= 0; i--) {
      if (up[u][i] != up[v][i]) {
        u = up[u][i];
        v = up[v][i];
      }
    }
    return up[u][0];
  };
  auto get_kth = [&](int u, int k) -> int {
    assert(dpth[u] >= k);
    int result = 0;
    for(int i = 0; i < B; i++, k >>= 1) {
      if (k & 1) {
        result = max(result, mxup[u][i]);
        u = up[u][i];
      }
    }
    return result;
  };
  for(auto [w, u, v, i] : edges) {
    int x = lca(u, v);
    answer[i] = mst + w - max(get_kth(u, dpth[u] - dpth[x]), get_kth(v, dpth[v] - dpth[x]));
  }
  for(int i = 0; i < m; i++) {
    cout << answer[i] << '\n';
  }
}
int main() {
  ios::sync_with_stdio(0); cin.tie(0);
  solve();
}