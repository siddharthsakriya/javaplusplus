// Sample 3: multi-threaded workload.
// Three concurrent threads, each with a distinct call chain, running at the same time.
public class Sample3 {
    public static void main(String[] args) throws InterruptedException {
        System.out.println("Sample3: multi-threaded workload starting...");

        Thread alpha = new Thread(Sample3::workerAlpha, "worker-alpha");
        Thread beta = new Thread(Sample3::workerBeta, "worker-beta");
        Thread gamma = new Thread(Sample3::workerGamma, "worker-gamma");

        alpha.start();
        beta.start();
        gamma.start();

        alpha.join();
        beta.join();
        gamma.join();

        System.out.println("Sample3: finished.");
    }

    static void workerAlpha() {
        spin(300);
    }

    static void workerBeta() {
        nestedBeta();
    }

    static void nestedBeta() {
        spin(250);
    }

    static void workerGamma() {
        nestedGamma1();
    }

    static void nestedGamma1() {
        nestedGamma2();
    }

    static void nestedGamma2() {
        spin(200);
    }

    static double spin(long durationMs) {
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
