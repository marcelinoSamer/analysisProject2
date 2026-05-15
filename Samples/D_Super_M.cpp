#include "bits/stdc++.h"
#pragma GCC optimize("O3,unroll-loops")
using namespace std;
typedef long long ll;
void solve() {
  int n, m; cin >> n >> m;
  vector<vector<int>> adj(n + 1);
  for(int i = 0; i < n - 1; i++) {
    int u, v; cin >> u >> v;
    adj[u].push_back(v);
    adj[v].push_back(u);
  }
  vector<bool> a(n + 1);
  for(int i = 0; i < m; i++) {
    int x; cin >> x;
    a[x] = 1;
  }
  vector<int> f(n + 1);
  vector<pair<int, int>> max_down(n + 1);
  auto dfs = [&](auto &&self, int u, int p) -> void {
    for(int v : adj[u]) {
      if (v != p) {
        self(self, v, u);
        if (f[v] || a[v]) {
          f[u] += f[v] + 2;
          if (max_down[v].first + 1 > max_down[u].first) {
            max_down[u] = {max_down[v].first + 1, max_down[u].first};
          } else if (max_down[v].first + 1 > max_down[u].second) {
            max_down[u].second = max_down[v].first + 1;
          }
        }
      }
    }
  }; dfs(dfs, 1, 0);
  int x = INT_MAX, y = INT_MAX;
  auto reroot = [&](auto &&self, int u, int p) -> void {
    // Get the answer for the current node
    if (a[u] && (f[u] - max_down[u].first < y || (f[u] - max_down[u].first <= y && u < x))) {
      y = f[u] - max_down[u].first;
      x = u;
    }
    for(int v : adj[u]) {
      if (v != p) {
        // Store old values
        int f1 = f[u], f2 = f[v];
        auto p1 = max_down[u], p2 = max_down[v];
        // Remove child's contribution from parent
        f[u] -= ((f[v] || a[v]) ? f[v] + 2 : 0);
        // Add parent's contribution to child
        if (f[u] || a[u]) {
          f[v] += f[u] + 2;
          int mx = (max_down[u].first == max_down[v].first + 1 ? max_down[u].second : max_down[u].first);
          if (mx + 1 > max_down[v].first) {
            max_down[v] = {mx + 1, max_down[v].second};
          } else if (mx + 1 > max_down[v].second) {
            max_down[v].second = mx + 1;
          }
        }
        self(self, v, u);
        // Restore old values
        f[u] = f1; f[v] = f2;
        max_down[u] = p1; max_down[v] = p2;
      }
    }
  }; reroot(reroot, 1, 0);
  cout << x << '\n' << y;
}
int main() {
  ios::sync_with_stdio(0); cin.tie(0);
  solve();
}