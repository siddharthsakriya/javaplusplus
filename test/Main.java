public class Main {
    public static void main(String[] args) throws Exception {
        System.out.println("Java: Starting work...");
        doWork();
        System.out.println("Java: Finished work.");
    }

    static void doWork() {
        methodA();
        methodB();
    }

    static void methodA() {
        nestedA1();
    }

    static void nestedA1() {
        nestedA2();
    }

    static void nestedA2() {
        busyLoop(200);
    }

    static void methodB() {
        nestedB1();
    }

    static void nestedB1() {
        busyLoop(200);
    }

    static double busyLoop(long durationMs) {
        long end = System.nanoTime() + durationMs * 1_000_000L;
        double acc = 0;
        while (System.nanoTime() < end) {
            for (int i = 0; i < 1000; i++) {
                acc += Math.sqrt(i);
            }
        }
        return acc;
    }
}
