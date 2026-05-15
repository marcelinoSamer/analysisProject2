#include "bits/stdc++.h"
#pragma GCC optimize("O3,unroll-loops")
using namespace std;
typedef long long ll;
const int B = 20;
template <typename T, typename Merge>
struct SPT { // Sparse Table
  int n, size;
  Merge merge;
  T ID;
  bool overlap;
  vector<vector<T>> t;
  vector<int> lg;
  SPT(int n_, Merge merge_, T ID_, bool overlap_) : n(n_), merge(merge_), ID(ID_), overlap(overlap_) {
    init();
  }
  template <typename U, typename Create>
  void build(const vector<U> &a, const Create& create) {
    for(int i = 0; i < n; i++) {
      t[0][i] = create(a[i]);
    }
    for(int k = 1; k <= size; k++) {
      for(int i = 0; i + (1<<(k - 1)) < n; i++) {
        t[k][i] = merge(t[k - 1][i], t[k - 1][i + (1<<(k - 1))]);
      }
    }
  }
  template <typename U>
  void build(const vector<U> &a) {
    build(a, [](const U& x) { return T(x); });
  }
  T get(int l, int r) {
    assert(l >= 0 && r < n && l <= r);
    if (overlap) return get_1(l, r);
    else return get_2(l, r);
  }
  private:
  void init() {
    assert(n > 0); size = 0;
    while((1<<size) < n) ++size;
    t.assign(size + 1, vector<T>(n, ID));
    lg.assign(n + 1, 0);
    for(int i = 2; i <= n; i++) lg[i] = lg[(i>>1)] + 1;
  }
  T get_1(int l, int r) {
    int msb = lg[r - l + 1];
    return merge(t[msb][l], t[msb][r - (1<<msb) + 1]);
  }
  T get_2(int l, int r) {
    T res = ID;
    int length = r - l + 1;
    for(int bit = size; bit >= 0 && l <= r; bit--) {
      if (length & (1<<bit)) {
        res = merge(res, t[bit][l]);
        length -= (1<<bit);
        l += (1<<bit);
      }
    }
    return res;
  }
};
void solve() {
  int n, m, q; cin >> n >> m >> q;
  vector<int> p(n);
  for(int i = 0; i < n; i++) {
    cin >> p[i]; --p[i];
  }
  vector<int> next(n, -1);
  for(int i = 0; i < n; i++) {
    next[p[i]] = p[(i + 1) % n];
  }
  vector<int> a(m);
  for(int i = 0; i < m; i++) {
    cin >> a[i]; --a[i];
  }
  vector<int> recent(n, -1);
  vector<array<int, B>> up(m);
  for(int i = 0; i < m; i++) {
    up[i].fill(-1);
  }
  vector<int> b(m, INT_MAX);
  for(int i = m - 1; i >= 0; i--) {
    up[i][0] = recent[next[a[i]]];
    for(int j = 1; j < B; j++) {
      int pp = up[i][j - 1];
      if (pp != -1) {
        up[i][j] = up[pp][j - 1];
      }
    }
    recent[a[i]] = i;
    int k = i;
    for(int j = B - 1; j >= 0; j--) {
      if (((n - 1)>>j) & 1) {
        if (up[k][j] == -1) {
          k = INT_MAX;
          break;
        } else {
          k = up[k][j];
        }
      }
    }
    b[i] = k;
  }
  auto merge = [](int x, int y) {return min(x, y);};
  SPT<int, decltype(merge)> table(m, merge, INT_MAX, 1);
  table.build(b);
  while(q--) {
    int l, r; cin >> l >> r;
    --l; --r;
    cout << (table.get(l, r) <= r ? '1' : '0');
  }
  cout << '\n';
}
int main() {
  ios::sync_with_stdio(0); cin.tie(0);
  solve();
}