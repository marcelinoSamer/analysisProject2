#include "bits/stdc++.h"
#pragma GCC optimize("O3,unroll-loops")
using namespace std;
typedef long long ll;
const int B = 2150, N = 1e5 + 5;
int a[N], b[N], c[N], f[N];
void solve() {
  int n; cin >> n;
  for(int i = 0; i < n; i++) {
    cin >> a[i];
  }
  for(int i = 0; i < n; i++) {
    cin >> b[i];
  }
  vector<ll> p(n + 1);
  for(int i = 1; i <= n; i++) {
    p[i] = p[i - 1] + b[i - 1];
  }
  for(int i = 0; i < n; i++) {
    c[i] = a[i];
  }
  int q; cin >> q;
  vector<array<int, 4>> qq;
  vector<array<int, 3>> up;
  for(int i = 0; i < q; i++) {
    int t, x, y; cin >> t >> x >> y;
    if (t == 1) {
      int j = int(up.size());
      qq.push_back({x - 1, y - 1, j, i - j});
    } else {
      up.push_back({x - 1, y, c[x - 1]});
      c[x - 1] = y;
    }
  }
  sort(qq.begin(), qq.end(), [](auto a1, auto a2) -> bool {
    auto [l1, r1, t1, _1] = a1;
    auto [l2, r2, t2, _2] = a2;
    if (l1 / B != l2 / B) return l1 < l2;
    if (r1 / B != r2 / B) return r1 < r2;
    return ((r1 / B) & 1 ? t1 < t2 : t2 < t1);
  });
  ll sum = 0;
  auto add = [&](int i) -> void {
    sum -= a[i] * p[f[a[i]]];
    ++f[a[i]];
    sum += a[i] * p[f[a[i]]];
  };
  auto remove = [&](int i) -> void {
    sum -= a[i] * p[f[a[i]]];
    --f[a[i]];
    sum += a[i] * p[f[a[i]]];
  };
  int L = 0, R = -1, T = 0;
  auto apply = [&](int t) -> void {
    auto [i, x, y] = up[t];
    if (i >= L && i <= R) remove(i);
    a[i] = x;
    if (i >= L && i <= R) add(i);
  };
  auto undo = [&](int t) -> void {
    auto [i, x, y] = up[t];
    if (i >= L && i <= R) remove(i);
    a[i] = y;
    if (i >= L && i <= R) add(i);
  };
  int q1 = int(qq.size());
  vector<ll> answer(q1);
  for(auto [l, r, t, i] : qq) {
    while(T < t) apply(T++);
    while(T > t) undo(--T);
    while(L > l) add(--L);
    while(R < r) add(++R);
    while(L < l) remove(L++);
    while(R > r) remove(R--);
    answer[i] = sum;
  }
  for(int i = 0; i < q1; i++) {
    cout << answer[i] << '\n';
  }
}
int main() {
  ios::sync_with_stdio(0); cin.tie(0);
  solve();
}