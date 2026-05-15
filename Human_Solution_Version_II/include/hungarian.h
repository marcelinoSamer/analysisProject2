#ifndef HUNGARIAN_H
#define HUNGARIAN_H

#include "config.h"

/*
Main Reference: https://cp-algorithms.com/graph/hungarian-algorithm.html#the-mathcalon3-algorithm
 
This implementation is based on the O(N^3) algorithm described in the cp-algorithms blog.
It solves the Minimum Weight Bipartite Matching (Assignment Problem).
 
To maximize weights (as required by the objective function), we will pass the negated weights
(-W) into this algorithm, turning our maximization problem into a minimization one.
*/
pair<ll, vector<int>> solve_hungarian(int n, int m, const vector<vector<ll>>& a) {
  vector<ll> u(n + 1, 0), v(m + 1, 0);
  vector<int> p(m + 1, 0), way(m + 1, 0);
  
  for (int i = 1; i <= n; ++i) {
    p[0] = i;
    int j0 = 0;
    vector<ll> minv(m + 1, INF);
    vector<char> used(m + 1, false);
    
    do {
      used[j0] = true;
      int i0 = p[j0];
      ll delta = INF;
      int j1 = 0;
      
      for (int j = 1; j <= m; ++j) {
        if (!used[j]) {
          ll cur = a[i0][j] - u[i0] - v[j];
          if (cur < minv[j]) {
            minv[j] = cur;
            way[j] = j0;
          }
          if (minv[j] < delta) {
            delta = minv[j];
            j1 = j;
          }
        }
      }
      
      for (int j = 0; j <= m; ++j) {
        if (used[j]) {
          u[p[j]] += delta;
          v[j] -= delta;
        } else {
          minv[j] -= delta;
        }
      }
      j0 = j1;
    } while (p[j0] != 0);
    
    do {
      int j1 = way[j0];
      p[j0] = p[j1];
      j0 = j1;
    } while (j0 != 0);
  }
  
  vector<int> ans(n + 1, 0);
  for (int j = 1; j <= m; ++j) {
    if (p[j] != 0) {
      ans[p[j]] = j;
    }
  }
  
  return {-v[0], ans};
}

#endif // HUNGARIAN_H
