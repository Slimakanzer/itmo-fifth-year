// Chandy-Misra algorithm implementation.
// Good Memory Usage
// Good Runtime

#include <functional>
#include <mutex>
#include <condition_variable>

using std::function;

class DiningPhilosophers {    
public:
    static constexpr uint32_t PhilosopherNum = 5;

    DiningPhilosophers() {
        for (auto i = 0; i < PhilosopherNum; i++)
            m_fork_available[i] = true;
    }

    void wantsToEat(int philosopher,
                    function<void()> pickLeftFork,
                    function<void()> pickRightFork,
                    function<void()> eat,
                    function<void()> putLeftFork,
                    function<void()> putRightFork) {
        
        uint32_t lfork = philosopher;
        uint32_t rfork = (philosopher + 1) % PhilosopherNum;

        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_condition[philosopher].wait(lock, [&](){
                return m_fork_available[lfork] && m_fork_available[rfork];
            });

            pickLeftFork();
            pickRightFork();
            m_fork_available[lfork] = false;
            m_fork_available[rfork] = false;
        }

        eat();

        {
            std::scoped_lock<std::mutex> lock(m_mutex);
            putLeftFork();
            putRightFork();
            m_fork_available[lfork] = true;
            m_fork_available[rfork] = true;
        }

        m_condition[philosopher].notify_one();
        m_condition[lfork].notify_one();
        m_condition[rfork].notify_one();   
    }

private:
    std::mutex m_mutex;
    std::condition_variable m_condition[PhilosopherNum];
    bool m_fork_available[PhilosopherNum];
};
