#include "bits/stdc++.h"
#pragma GCC optimize("O3,unroll-loops")
using namespace std;
typedef long long ll;
const int N = 1e5 + 5, M = 1e5 + 5, W = 12;
vector<int> adj[N];
string s[N];
int color[N][W];
int n, m, w;
bool dfs(int u, int d) {
  color[u][d] = 1;
  int nd = (d + 1) % w;
  for(int v : adj[u]) {
    if (s[v][nd] == 'x') {
      continue;
    }
    if (color[v][nd] == 0) {
      if (dfs(v, nd)) {
        return true;
      }
    } else if (color[v][nd] == 1) {
      return true;
    }
  }
  color[u][d] = 2;
  return false;
}
void solve() {
  cin >> n >> m;
  for(int u = 0; u < n; u++) {
    adj[u].clear();
    adj[u].push_back(u);
    for(int i = 0; i < W; i++) {
      color[u][i] = 0;
    }
  }
  for(int i = 0; i < m; i++) {
    int u, v; cin >> u >> v;
    --u; --v;
    adj[u].push_back(v);
    adj[v].push_back(u);
  }
  cin >> w;
  for(int i = 0; i < n; i++) {
    cin >> s[i];
  }
  for(int u = 0; u < n; u++) {
    if (color[u][0] == 0 && s[u][0] == 'o') {
      if (dfs(u, 0)) {
        return void(cout << "Yes\n");
      }
    }
  }
  cout << "No\n";
}
int main() {
  ios::sync_with_stdio(0); cin.tie(0);
  int t; cin >> t; while(t--)
  solve();
}