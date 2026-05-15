#include "bits/stdc++.h"
#pragma GCC optimize("O3,unroll-loops")
using namespace std;
typedef long long ll;
const int A = 1e5 + 5, B = 2150, C = 20;
int f[A];
bool withme[A];
void solve() {
  int n, q; cin >> n >> q;
  vector<int> a(n);
  for(int i = 0; i < n; i++) {
    cin >> a[i];
  }
  vector<vector<int>> adj(n);
  for(int i = 0; i < n - 1; i++) {
    int u, v; cin >> u >> v;
    adj[u].push_back(v);
    adj[v].push_back(u);
  }
  int timer = -1;
  vector<int> st(n), ft(n), flattend(n + n);
  vector<int> depth(n);
  vector<array<int, C>> up(n);
  for(int i = 0; i < n; i++) {
    up[i].fill(-1);
  }
  auto dfs = [&](auto &&self, int u, int p) -> void {
    ++timer;
    st[u] = timer;
    flattend[timer] = u;
    if (p != -1) {
      up[u][0] = p;
      depth[u] = depth[p] + 1;
    }
    for(int i = 1; i < C; i++) {
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
    ++timer;
    ft[u] = timer;
    flattend[timer] = u;
  }; dfs(dfs, 0, -1);
  auto lca = [&](int u, int v) -> int {
    if (depth[u] < depth[v]) {
      swap(u, v);
    }
    for(int i = C - 1; i >= 0; i--) {
      if (depth[u] - (1<<i) >= depth[v]) {
        u = up[u][i];
      }
    }
    if (u == v) {
      return u;
    }
    for(int i = C - 1; i >= 0; i--) {
      if (up[u][i] != up[v][i]) {
        u = up[u][i];
        v = up[v][i];
      }
    }
    return up[u][0];
  };
  auto c = a;
  vector<array<int, 4>> qq;
  vector<array<int, 3>> upd;
  for(int i = 0; i < q; i++) {
    int t, x, y; cin >> t >> x >> y;
    if (t == 1) {
      upd.push_back({x, y, c[x]});
      c[x] = y;
    } else {
      int j = int(upd.size());
      if (st[x] > st[y]) swap(x, y);
      int z = lca(x, y);
      if (z == x) {
        qq.push_back({st[x], st[y], j, i - j});
      } else {
        qq.push_back({ft[x], st[y], j, i - j});
      }
    }
  }
  [[maybe_unused]] int q1 = int(qq.size()), q2 = int(upd.size());
  sort(qq.begin(), qq.end(), [](auto a1, auto a2) -> bool {
    auto [l1, r1, t1, _1] = a1;
    auto [l2, r2, t2, _2] = a2;
    if (l1 / B != l2 / B) return l1 < l2;
    if (r1 / B != r2 / B) return r1 < r2;
    return ((r1 / B) & 1 ? t1 < t2 : t2 < t1);
  });
  ll sum = 0;
  auto add = [&](int i) -> void {
    sum += f[a[flattend[i]]];
    ++f[a[flattend[i]]];
  };
  auto remove = [&](int i) -> void {
    --f[a[flattend[i]]];
    sum -= f[a[flattend[i]]];
  };
  auto toggle = [&](int i) -> void {
    withme[flattend[i]] ^= 1;
    if (withme[flattend[i]]) add(i);
    else remove(i);
  };
  int L = 0, R = -1, T = 0;
  auto apply = [&](int t) -> void {
    auto [i, x, y] = upd[t];
    if (withme[i]) remove(st[i]);
    a[i] = x;
    if (withme[i]) add(st[i]);
  };
  auto undo = [&](int t) -> void {
    auto [i, x, y] = upd[t];
    if (withme[i]) remove(st[i]);
    a[i] = y;
    if (withme[i]) add(st[i]);
  };
  vector<ll> answer(q1);
  for(auto [l, r, t, i] : qq) {
    while(T < t) apply(T++);
    while(T > t) undo(--T);
    while(L > l) toggle(--L);
    while(R < r) toggle(++R);
    while(L < l) toggle(L++);
    while(R > r) toggle(R--);
    int z = lca(flattend[l], flattend[r]);
    if (z != flattend[l]) {
      toggle(st[z]);
    }
    answer[i] = sum;
    if (z != flattend[l]) {
      toggle(st[z]);
    }
  }
  for(int i = 0; i < q1; i++) {
    cout << answer[i] << '\n';
  }
}
int main() {
  ios::sync_with_stdio(0); cin.tie(0);
  solve();
}