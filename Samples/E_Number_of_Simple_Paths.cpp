#include "bits/stdc++.h"
#pragma GCC optimize("O3,unroll-loops")
using namespace std;
typedef long long ll;
void solve() {
  int n; cin >> n;
  vector<vector<int>> adj(n);
  for(int i = 0; i < n; i++) {
    int u, v; cin >> u >> v;
    --u; --v;
    adj[u].push_back(v);
    adj[v].push_back(u);
  }
  vector<int> sz(n), par(n, -1);
  vector<bool> visited(n); visited[0] = 1;
  ll answer = 0;
  int a = -1, b = -1;
  auto dfs = [&](auto &&self, int u, int p) -> void {
    sz[u] = 1;
    par[u] = p;
    ll x = 0, y = 0;
    for(int v : adj[u]) {
      if (!visited[v]) {
        visited[v] = 1;
        self(self, v, u);
        sz[u] += sz[v];
        x += sz[v];
        y += 1LL * sz[v] * sz[v];
      } else if (v != p && a == -1) {
        a = v; b = u;
      }
    }
    answer += sz[u] - 1;
    answer += (1LL * x * x - y) / 2;
  }; dfs(dfs, 0, -1);
  int cur = b, prv = -1;
  int x = sz[b], y = 0;
  ll s2 = 0;
  while(par[cur] != a) {
    prv = cur;
    cur = par[cur];
    int d = sz[cur] - sz[prv];
    y += d;
    s2 += 1LL * d * d;
  }
  answer += (1LL * y * y - s2) / 2;
  int z = n - x - y;
  answer += (1LL * (x + y + z) * (x + y + z) - 1LL * x * x - 1LL * y * y - 1LL * z * z) / 2;
  cout << answer << '\n';
}
int main() {
  ios::sync_with_stdio(0); cin.tie(0);
  int t; cin >> t; while(t--)
  solve();
}