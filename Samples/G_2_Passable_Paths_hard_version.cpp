#include "bits/stdc++.h"
#pragma GCC optimize("O3,unroll-loops")
using namespace std;
typedef long long ll;
const int B = 19;
void solve() {
  int n; cin >> n;
  vector<vector<int>> adj(n + 1);
  for(int i = 0; i < n - 1; i++) {
    int u, v; cin >> u >> v;
    adj[u].push_back(v);
    adj[v].push_back(u);
  }
  vector<int> depth(n + 1); depth[0] = -1;
  vector<array<int, B>> up(n + 1);
  for(int i = 0; i <= n; i++) {
    up[i].fill(-1);
  }
  auto dfs = [&](auto &&self, int u, int p) -> void {
    up[u][0] = p;
    depth[u] = depth[p] + 1;
    for(int i = 1; i < B; i++) {
      int pp = up[u][i - 1];
      if (pp != -1) {
        up[u][i] = up[pp][i - 1];
      }
    }
    for(int v : adj[u]) {
      if (v != p) {
        self(self, v, u);
      }
    }
  }; dfs(dfs, 1, 0);
  auto lca = [&](int u, int v) -> int {
    if (depth[u] < depth[v]) {
      swap(u, v);
    }
    for(int i = B - 1; i >= 0; i--) {
      if (depth[u] - (1<<i) >= depth[v]) {
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
  auto distance = [&](int u, int v) -> int {
    return depth[u] + depth[v] - 2 * depth[lca(u, v)];
  };
  auto get_kth = [&](int u, int k) -> int {
    assert(depth[u] >= k);
    for(int i = 0; i < B; i++, k >>= 1) {
      if (k & 1) {
        u = up[u][i];
      }
    }
    return u;
  };
  auto check = [&](vector<int> p) -> bool {
    sort(p.begin(), p.end(), [&](int u, int v) {
      return depth[u] > depth[v];
    });
    int m = int(p.size());
    for(int i = 1; i < m; i++) {
      if (get_kth(p[i - 1], depth[p[i - 1]] - depth[p[i]]) != p[i]) {
        return false;
      }
    }
    return true;
  };
  int q; cin >> q;
  while(q--) {
    int k; cin >> k;
    int x = -1;
    vector<int> a(k);
    for(int i = 0; i < k; i++) {
      cin >> a[i];
      if (x == -1 || depth[x] > depth[a[i]]) {
        x = a[i];
      }
    }
    if (k == 1) {
      cout << "YES\n";
      continue;
    }
    int y = -1;
    for(int i = 0; i < k; i++) {
      if (lca(a[i], x) != x) {
        if (y == -1 || depth[y] > depth[a[i]]) {
          y = a[i];
        }
      }
    }
    bool bad = false;
    if (y == -1) {
      for(int i = 0; i < k; i++) {
        if (a[i] == x) continue;
        if (y == -1 || depth[y] > depth[a[i]]) {
          y = a[i];
        }
      }
      for(int i = 0; i < k; i++) {
        if (a[i] == x) continue;
        int u = lca(y, a[i]);
        if (u != y && u != x) {
          bad = true;
          break;
        }
      }
      swap(y, x);
    }
    // cout << x << ' ' << y << endl;
    vector<int> v1, v2;
    for(int i = 0; i < k; i++) {
      if (lca(x, a[i]) == x) {
        v1.push_back(a[i]);
      } else if (lca(y, a[i]) == y) {
        v2.push_back(a[i]);
      } else {
        bad = true;
        break;
      }
    }
    // cout << "||| " << bad << endl;
    // for(int u : v1) cout << u << ' ';
    // cout << endl;
    // for(int u : v2) cout << u << ' ';
    // cout << endl;
    if (bad || !check(v1) || !check(v2)) {
      cout << "NO\n";
    } else {
      cout << "YES\n";
    }
  }
}
int main() {
  ios::sync_with_stdio(0); cin.tie(0);
  solve();
}