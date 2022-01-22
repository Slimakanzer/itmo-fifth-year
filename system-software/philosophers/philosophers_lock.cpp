// Implementation with multi-lock. Very bad idea...

#include <mutex>

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
		
        std::scoped_lock lock(m_forks[lfork], m_forks[rfork]);
        
        pickLeftFork();
        pickRightFork();

        eat();

        putRightFork();
        putLeftFork();
    }

private:
    std::mutex m_forks[PhilosopherNum];
};