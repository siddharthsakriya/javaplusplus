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
        for (int i = 0; i < 1000; i++) { Math.sqrt(i); }
    }

    static void methodB() {
        for (int i = 0; i < 1000; i++) { Math.sqrt(i); }
    }
}