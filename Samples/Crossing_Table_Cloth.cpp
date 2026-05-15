#include "bits/stdc++.h"
#pragma GCC optimize("O3,unroll-loops")
using namespace std;
typedef long long ll;
void solve() {
  int n, m; cin >> n >> m;
  vector<set<int>> atl(n), atr(n);
  vector<int> p(n), s(n, n);
  vector<map<int, int>> c(n);
  for(int i = 0; i < m; i++) {
    int l, r; cin >> l >> r;
    --l; --r;
    ++c[l][r];
    p[r] = max(p[r], l);
    s[l] = min(s[l], r);
    atl[l].insert(r);
    atr[r].insert(l);
  }
  for(int i = 1; i < n; i++) {
    p[i] = max(p[i], p[i - 1]);
  }
  for(int i = n - 2; i >= 0; i--) {
    s[i] = min(s[i], s[i + 1]);
  }
  int q; cin >> q;
  for(int i = 0; i < q; i++) {
    int l, r; cin >> l >> r;
    --l; --r;
    if (atl[l].empty() || atr[r].empty()) {
      cout << "No\n";
    } else {
      if (atl[l].find(r) == atl[l].end()) {
        auto it1 = atl[l].upper_bound(r);
        auto it2 = atr[r].upper_bound(l);
        if (it2 == atr[r].end() || it1 == atl[l].begin()) {
          cout << "No\n";
        } else {
          it1 = prev(it1);
          cout << (*it1 >= *it2 - 1 ? "Yes" : "No") << '\n';
        }
      } else {
        cout << (p[r] > l || s[l] < r || (c[l].find(r) != c[l].end() && c[l][r] > 1) ? "Yes" : "No") << '\n';
      }
    }
  }
}
int main() {
  ios::sync_with_stdio(0); cin.tie(0);
  solve();
}