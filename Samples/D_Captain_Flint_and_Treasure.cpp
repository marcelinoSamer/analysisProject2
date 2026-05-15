#include "bits/stdc++.h"
#pragma GCC optimize("O3,unroll-loops")
using namespace std;
typedef long long ll;
void solve() {
  int n; cin >> n;
  vector<ll> a(n);
  for(int i = 0; i < n; i++) {
    cin >> a[i];
  }
  vector<vector<int>> adj(n);
  vector<int> deg(n);
  for(int i = 0; i < n; i++) {
    int x; cin >> x;
    if (x != -1) {
      adj[x - 1].push_back(i);
      ++deg[i];
    }
  }
  ll answer = 0;
  stack<int> s;
  queue<int> q;
  auto dfs = [&](auto &&self, int u) -> void {
    for(int v : adj[u]) {
      self(self, v);
      answer += a[v];
      if (a[v] > 0) {
        a[u] += a[v];
        q.push(v);
      } else {
        s.push(v);
      }
    }
  };
  vector<int> order;
  auto flush1 = [&]() -> void {
    while(!q.empty()) {
      order.push_back(q.front());
      q.pop();
    }
  };
  auto flush2 = [&]() -> void {
    while(!s.empty()) {
      order.push_back(s.top());
      s.pop();
    }
  };
  for(int u = 0; u < n; u++) {
    if (deg[u] == 0) {
      ll before = a[u];
      dfs(dfs, u);
      ll after = a[u];
      if (before > after) {
        order.push_back(u);
        flush1();
        flush2();
      } else {
        flush1();
        order.push_back(u);
        flush2();
      }
      answer += max(before, after);
    }
  }
  cout << answer << '\n';
  for(int x : order) cout << x + 1 << ' ';
}
int main() {
  ios::sync_with_stdio(0); cin.tie(0);
  solve();
}