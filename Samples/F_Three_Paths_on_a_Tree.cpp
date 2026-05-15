#include "bits/stdc++.h"
#pragma GCC optimize("O3,unroll-loops")
using namespace std;
typedef long long ll;
void solve() {
  int n; cin >> n;
  vector<vector<int>> adj(n);
  for(int i = 0; i < n - 1; i++) {
    int u, v; cin >> u >> v;
    --u; --v;
    adj[u].push_back(v);
    adj[v].push_back(u);
  }
  vector<int> par, dis;
  auto farthest = [&](int s, int n) -> int {
    static const int INF = 1e6; // Max distance
    dis.assign(n, INF); dis[s] = 0;
    par.assign(n, -1);
    vector<bool> vis(n);
    queue<int> q; q.push(s);
    vis[s] = 1; int last = s;
    while (!q.empty()) {
      int u = q.front(); q.pop();
      for (int v: adj[u]) {
        if (vis[v]) continue;
        dis[v] = dis[u] + 1;
        q.push(v); vis[v] = 1;
        par[v] = u;
      }
      last = u;
    }
    return last;
  };
  auto dfs = [&](auto &&self, int u, int p) -> pair<int, int> {
    pair<int, int> result = {0, u};
    for(int v : adj[u]) {
      if (v != p) {
        result = max(result, self(self, v, u));
      }
    }
    ++result.first;
    return result;
  };
  int x = farthest(0, n);
  int y = farthest(x, n);
  int base = dis[y], extra = 0, z = 0;
  while(z == x || z == y) ++z;
  int cur = y, prv = -1;
  while(cur != x) {
    for(int v : adj[cur]) {
      if (v != par[cur] && v != prv) {
        auto [a, b] = dfs(dfs, v, cur);
        if (a > extra) {
          z = b;
          extra = a;
        }
      }
    }
    prv = cur;
    cur = par[cur];
  }
  cout << base + extra << '\n';
  cout << x + 1 << ' ' << y + 1 << ' ' << z + 1 << '\n';
}
int main() {
  ios::sync_with_stdio(0); cin.tie(0);
  solve();
}