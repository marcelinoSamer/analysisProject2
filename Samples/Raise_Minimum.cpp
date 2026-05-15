#include "bits/stdc++.h"
#pragma GCC optimize("O3,unroll-loops")
using namespace std;
typedef long long ll;
void solve() {
  int n; cin >> n;
  ll k; cin >> k;
  vector<ll> a(n);
  for(int i = 0; i < n; i++) {
    cin >> a[i];
  }
  auto good = [&](ll mid) -> bool {
    auto b = a;
    ll kk = k;
    for(int i = 1; i <= n && kk >= 0; i++) {
      ll d = max(0LL, mid - b[i - 1]);
      kk -= (d + i - 1) / i;
    }
    return (kk >= 0);
  };
  ll l = 1, r = 2e18, b = 1;
  while(l <= r) {
    ll mid = l + (r - l) / 2;
    if (good(mid)) {
      l = mid + 1;
      b = mid;
    } else {
      r = mid - 1;
    }
  }
  cout << b << '\n';
}
int main() {
  ios::sync_with_stdio(0); cin.tie(0);
  solve();
}