#include "bits/stdc++.h"
#pragma GCC optimize("O3,unroll-loops")
using namespace std;
typedef long long ll;
const int MOD = 998244353;
int add(int a, int b) {return ((ll)a + b) % MOD;}
int sub(int a, int b) {return (a - b + MOD) % MOD;}
int mul(int a, int b) {return (a * 1LL * b) % MOD;}
void solve() {
  string s; cin >> s;
  int n = int(s.length());
  int answer = 0;
  for(int i = 0; i < n; ) {
    int j = i + 1;
    while(j < n && s[j] != s[j - 1]) {
      ++j;
    }
    int length = j - i;
    answer = add(answer, (1LL * length * (length + 1) / 2) % MOD);
    i = j;
  }
  cout << answer;
}
int main() {
  ios::sync_with_stdio(0); cin.tie(0);
  solve();
}