#include "bits/stdc++.h"
#pragma GCC optimize("O3,unroll-loops")
using namespace std;
typedef long long ll;
const ll INF = 1e14;
void solve() {
  int n, c; cin >> n >> c;
  vector<vector<array<int, 2>>> adj(n + 1);
  vector<int> all(n - 1);
  for(int i = 0; i < n - 1; i++) {
    int u, v, w; cin >> u >> v >> w;
    adj[u].push_back({v, w});
    adj[v].push_back({u, w});
    all[i] = w;
  }
  if (n == 1) {
    return void(cout << "0\n");
  }
  vector<int> sorted = all;
  sort(sorted.begin(), sorted.end());
  sorted.erase(unique(sorted.begin(), sorted.end()), sorted.end());
  int m = sorted.size();
  for(int u = 1; u <= n; u++) {
    for(auto &[v, w] : adj[u]) {
      w = int(lower_bound(sorted.begin(), sorted.end(), w) - sorted.begin());
    }
  }
  vector<vector<ll>> dp(n + 1);
  auto dfs = [&](auto &&self, int u, int p, int pw) -> void {
    int mx = 0;
    ll children_free = 0;
    for(auto [v, w] : adj[u]) {
      mx = max(mx, w);
      if (v != p) {
        self(self, v, u, w);
        ll mn = INF;
        for(int i = w; i < m; i++) {
          mn = min(mn, dp[v][i]);
        }
        children_free += mn;
      }
    }
    dp[u].assign(m, INF);
    for(int i = mx; i < m; i++) dp[u][i] = 0;
    for(auto [v, w] : adj[u]) {
      if (v != p) {
        for(int i = mx; i < m; i++) {
          dp[u][i] += dp[v][i];
        }
        vector<ll>().swap(dp[v]);
      }
    }
    for(int i = mx; i < m; i++) {
      int to_parent = (p ? sorted[i] - sorted[pw] : 0);
      dp[u][i] += to_parent;
    }
    ll C = 1LL * c * int(adj[u].size());
    for(int i = pw; i < m; i++) {
      int to_parent = (p ? sorted[i] - sorted[pw] : 0);
      dp[u][i] = min(dp[u][i], to_parent + C + children_free);
    }
  }; dfs(dfs, 1, 0, 0);
  cout << *min_element(dp[1].begin(), dp[1].end());
}
int main() {
  ios::sync_with_stdio(0); cin.tie(0);
  solve();
}