//Test case - Common subexpression elimination
// (no CSE test case was given, so this one is written for the assignment)
int compute(int a, int b)
{
  int x = a + b;
  int y = a + b; // same as x -> reuse x
  return x * y;
}
