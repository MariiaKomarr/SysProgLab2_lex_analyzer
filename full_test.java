#define

@Deprecated
public class Demo {
  // line comment
  /* block comment */
  int a = 42;
  long big = 42L;
  double d1 = 3.14;
  double d2 = .5;
  float f = 1e-3F;
  int hex = 0xDEAD_BEEF;
  int bin = 0b1010_0110;
  int oct = 0777;
  String s = "Hi\n";
  char c = '\n';
  boolean t = true, f2 = false;
  Object o = null;

  void ops() {
    int[] arr = new int[10];
    int x = 1_000;
    x++; x--; x += 2; x -= 3; x *= 4; x /= 5; x %= 6;
    x &= 7; x |= 8; x ^= 9;
    int y = x << 1; y >>= 1; y >>>= 2; // >>, >>>, >>=, >>>=
    boolean b = (x == y) ? (x >= y) : (x < y && x != y || x <= y);
    java.util.function.Function<String,Integer> p = Integer::parseInt;
    var lam = (int z) -> z + 1;
    @SuppressWarnings("unused") int ann = 0;
    System.out.println(s + c + d1 + d2);
  }
}

