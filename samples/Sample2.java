// Sample 2: recursive workload, single thread.
// Naive recursive Fibonacci produces a deep, self-similar call tree.
// Repeated in a time-boxed loop so total runtime is predictable regardless
// of how fast the JIT gets at any single fib() call.
public class Sample2 {
    public static void main(String[] args) {
        System.out.println("Sample2: recursive workload starting...");
        long end = System.nanoTime() + 300_000_000L;
        long result = 0;
        while (System.nanoTime() < end) {
            result += fib(28);
        }
        System.out.println("fib(28) sum = " + result);
        System.out.println("Sample2: finished.");
    }

    static long fib(int n) {
        if (n <= 1) return n;
        return fib(n - 1) + fib(n - 2);
    }
}
