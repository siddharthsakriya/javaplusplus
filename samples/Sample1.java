// Sample 1: sequential nested workload, single thread.
// Three independent call chains of varying depth off of main().
public class Sample1 {
    public static void main(String[] args) {
        System.out.println("Sample1: sequential nested workload starting...");
        pipelineStageA();
        pipelineStageB();
        pipelineStageC();
        System.out.println("Sample1: finished.");
    }

    static void pipelineStageA() {
        validate();
    }

    static void validate() {
        spin(150);
    }

    static void pipelineStageB() {
        transform();
    }

    static void transform() {
        normalize();
    }

    static void normalize() {
        spin(150);
    }

    static void pipelineStageC() {
        spin(150);
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
