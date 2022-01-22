// Implementation with advanced multi-lock algorithm.
// Bad Memory Usage
// Good Runtime

#include <mutex>
#include <thread>

template <class L0, class L1>
void my_lock(L0& l0, L1& l1)
{
    while (true)
    {
        {
            std::unique_lock<L0> u0(l0);
            if (l1.try_lock())
            {
                u0.release();
                break;
            }
        }
        std::this_thread::yield();
        {
            std::unique_lock<L1> u1(l1);
            if (l0.try_lock())
            {
                u1.release();
                break;
            }
        }
        std::this_thread::yield();
    }
}

class DiningPhilosophers {    
public:
    static constexpr uint32_t PhilosopherNum = 5;

    DiningPhilosophers() {
    }

    void wantsToEat(int philosopher,
                    function<void()> pickLeftFork,
                    function<void()> pickRightFork,
                    function<void()> eat,
                    function<void()> putLeftFork,
                    function<void()> putRightFork) {
        uint32_t lfork = philosopher;
        uint32_t rfork = (philosopher + 1) % PhilosopherNum;
		
        std::unique_lock<std::mutex> llock = std::unique_lock(m_forks[lfork], std::defer_lock);
        std::unique_lock<std::mutex> rlock = std::unique_lock(m_forks[rfork], std::defer_lock);

        my_lock(llock, rlock);
        
        pickLeftFork();
        pickRightFork();

        eat();

        putRightFork();
        putLeftFork();
    }

private:
    std::mutex m_forks[PhilosopherNum];
};