#include "bits/stdc++.h"
// #pragma GCC optimize("O3,unroll-loops")
using namespace std;
typedef long long ll;
const int N = 5e5 + 5, B = 710;
int a[N];
vector<int> at[B];
void solve() {
  int n; cin >> n;
  for(int i = 0; i < n; i++) {
    cin >> a[i];
    at[i / B].push_back(a[i]);
  }
  for(int i = 0; i < B; i++) {
    sort(at[i].rbegin(), at[i].rend());
  }
  auto get = [&](int b, int c) -> int {
    int l = 0, r = int(at[b].size()) - 1, best = -1;
    while(l <= r) {
      int mid = l + (r - l) / 2;
      if (at[b][mid] < c) r = mid - 1;
      else l = mid + 1, best = mid;
    }
    return best + 1;
  };
  int q; cin >> q;
  vector<array<int, 4>> qq(q);
  for(int i = 0; i < q; i++) {
    int t; cin >> t;
    if (t == 0) {
      int l, r, c; cin >> l >> r >> c;
      --l; --r;
      int cl = l / B, cr = r / B;
      int answer = 0;
      if (cl == cr) {
        for(int start = l; start <= r; start++) {
          if (a[start] >= c) ++answer;
        }
      } else {
        for(int start = l, end = (cl + 1) * B; start < end; start++) {
          if (a[start] >= c) ++answer;
        }
        for(int start = cl + 1; start < cr; start++) {
          answer += get(start, c);
        }
        for(int start = cr * B; start <= r; start++) {
          if (a[start] >= c) ++answer ;
        }
      }
      cout << answer << '\n';
    } else {
      int j, x; cin >> j >> x;
      --j;
      j -= 10;
      int k = j / B;
      int l = 0, r = int(at[k].size()) - 1, b = -1;
      while(l <= r) {
        int mid = l + (r - l) / 2;
        if (at[k][mid] > a[j]) l = mid + 1;
        else if (at[k][mid] < a[j]) r = mid - 1;
        else {b = mid; break;}
      }
      a[j] = at[k][b] = x;
      sort(at[k].rbegin(), at[k].rend());
    }
  }
  
}
int main() {
  ios::sync_with_stdio(0); cin.tie(0);
  solve();
}