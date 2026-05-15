#include "bits/stdc++.h"
#pragma GCC optimize("O3,unroll-loops")
using namespace std;
typedef long long ll;
void solve() {
  int x; cin >> x;
  cout << (x >= 3 && x <= 18 ? "Yes" : "No");
}
int main() {
  ios::sync_with_stdio(0); cin.tie(0);
  solve();
}