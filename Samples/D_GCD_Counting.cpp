#include "bits/stdc++.h"
#pragma GCC optimize("O3,unroll-loops")
using namespace std;
typedef long long ll;
const int N = 2e5 + 7;
vector<int> pr;
int lp[N] {};
vector<int> factors[N];
void linear_sieve() {
  // lp[0] = lp[1] = 0
  for (int i = 2; i < N; ++i) {
    if (lp[i] == 0) {
      lp[i] = i;
      pr.push_back(i);
    }
    for (int j = 0; i * pr[j] < N; ++j) {
      lp[i * pr[j]] = pr[j];
      if (pr[j] == lp[i]) {
        break;
      }
    }
  }
}
void solve() {
  int n; cin >> n;
  vector<int> a(n + 1);
  for(int i = 1; i <= n; i++) {
    cin >> a[i];
  }
  vector<vector<int>> adj(n + 1);
  for(int i = 0; i < n - 1; i++) {
    int u, v; cin >> u >> v;
    adj[u].push_back(v);
    adj[v].push_back(u);
  }
  vector<map<int, int>> c(n + 1);
  int answer = 0;
  auto dfs = [&](auto &&self, int u, int p) -> void {
    map<int, pair<int, int>> mx;
    for(int v : adj[u]) {
      if (v != p) {
        self(self, v, u);
        for(int f : factors[a[u]]) {
          if (c[v].find(f) != c[v].end()) {
            if (c[v][f] > mx[f].first) {
              mx[f].second = mx[f].first;
              mx[f].first = c[v][f];
            } else if (c[v][f] > mx[f].second) {
              mx[f].second = c[v][f];
            }
          }
        }
      }
    }
    for(int f : factors[a[u]]) {
      c[u][f] = mx[f].first + 1;
      answer = max(answer, mx[f].first + mx[f].second + 1);
    }
  }; dfs(dfs, 1, 0);
  cout << answer;
}
int main() {
  linear_sieve();
  for(int i = 2; i < N; i++) {
    int j = i;
    while(lp[j]) {
      factors[i].push_back(lp[j]);
      while(lp[j] == factors[i].back()) {
        j /= lp[j];
      }
    }
  }
  ios::sync_with_stdio(0); cin.tie(0);
  solve();
}