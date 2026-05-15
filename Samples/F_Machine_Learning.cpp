#include "bits/stdc++.h"
#pragma GCC optimize("O3,unroll-loops")
using namespace std;
typedef long long ll;
const int B = 2150;
void solve() {
  int n, q; cin >> n >> q;
  vector<int> all(n + q), a(n);
  for(int i = 0; i < n; i++) {
    cin >> a[i];
    all[i] = a[i];
  }
  vector<array<int, 4>> qq;
  vector<array<int, 3>> up;
  auto b = a;
  for(int i = 0, j = 0; i < q; i++) {
    int t, x, y; cin >> t >> x >> y;
    if (t == 1) {
      qq.push_back({x - 1, y - 1, j, i - j});
    } else {
      up.push_back({x - 1, y, b[x - 1]});
      b[x - 1] = y;
      all[n + j] = y;
      ++j;
    }
  }
  int q1 = int(qq.size()), q2 = int(up.size());
  vector<int> sorted = all;
  sort(sorted.begin(), sorted.end());
  sorted.erase(unique(sorted.begin(), sorted.end()), sorted.end());
  int m = sorted.size();
  for (int i = 0; i < n; i++) {
    a[i] = int(lower_bound(sorted.begin(), sorted.end(), a[i]) - sorted.begin());
  }
  for(int i = 0; i < q2; i++) {
    up[i][1] = int(lower_bound(sorted.begin(), sorted.end(), up[i][1]) - sorted.begin());
    up[i][2] = int(lower_bound(sorted.begin(), sorted.end(), up[i][2]) - sorted.begin());
  }
  sort(qq.begin(), qq.end(), [](auto a1, auto a2) {
    auto [l1, r1, t1, _1] = a1;
    auto [l2, r2, t2, _2] = a2;
    if (l1 / B != l2 / B) return l1 < l2;
    if (r1 / B != r2 / B) return r1 < r2;
    return ((r1 / B) & 1 ? t1 < t2 : t2 < t1);
  });
  vector<int> f(m), ff(n + 1); ff[0] = n + n;
  auto add = [&](int i) -> void {
    --ff[f[a[i]]];
    ++f[a[i]];
    ++ff[f[a[i]]];
  };
  auto remove = [&](int i) -> void {
    --ff[f[a[i]]];
    --f[a[i]];
    ++ff[f[a[i]]];
  };
  int L = 0, R = -1, T = 0;
  auto apply = [&](int i) -> void {
    auto [j, x, y] = up[i];
    if (j >= L && j <= R) remove(j);
    a[j] = x;
    if (j >= L && j <= R) add(j);
  };
  auto undo = [&](int i) -> void {
    auto [j, x, y] = up[i];
    if (j >= L && j <= R) remove(j);
    a[j] = y;
    if (j >= L && j <= R) add(j);
  };
  vector<int> answer(q1);
  for(auto [l, r, t, i] : qq) {
    while(T < t) {apply(T); ++T;}
    while(T > t) {--T; undo(T);}
    while(L > l) {--L; add(L);}
    while(R < r) {++R; add(R);}
    while(L < l) {remove(L); ++L;}
    while(R > r) {remove(R); --R;}
    while(ff[answer[i]]) ++answer[i];
  }
  for(int i = 0; i < q1; i++) {
    cout << answer[i] << '\n';
  }
}
int main() {
  ios::sync_with_stdio(0); cin.tie(0);
  solve();
}