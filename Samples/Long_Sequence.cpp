#include "bits/stdc++.h"
#pragma GCC optimize("O3,unroll-loops")
using namespace std;
typedef long long ll;
void solve() {
  int n; cin >> n;
  ll k; cin >> k;
  vector<int> l(n);
  vector<vector<int>> a(n);
  for(int i = 0; i < n; i++) {
    cin >> l[i];
    a[i].resize(l[i]);
    for(int j = 0; j < l[i]; j++) {
      cin >> a[i][j];
    }
  }
  vector<int> c(n);
  for(int i = 0; i < n; i++) {
    cin >> c[i];
  }
  for(int i = 0; i < n; i++) {
    if (k - 1LL * c[i] * l[i] > 0) {
      k -= 1LL * c[i] * l[i];
    } else {
      k %= l[i];
      if (k == 0) k = l[i];
      return void(cout << a[i][k - 1] << '\n');
    }
  }
  assert(false);
}
int main() {
  ios::sync_with_stdio(0); cin.tie(0);
  solve();
}