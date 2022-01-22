#include <iostream>
#include <cstdlib>
#include <iomanip>
#include <vector>
#include <numeric>

#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

#define PTHREAD_CHECK(x) if(x != 0) { return  1; }

enum DataState
{
    Ready,
    Empty,
    End,
};

struct ThreadData
{
    ThreadData(unsigned num_consumers, unsigned consumer_sleep_limit, bool debug)
        : num_consumers(num_consumers), consumer_sleep_limit(consumer_sleep_limit), debug(debug)
    {
        consumers = std::vector<pthread_t>(num_consumers);
        consumers_psum = std::vector<int>(num_consumers, 0);
    }

    const unsigned num_consumers = 0;
    const unsigned consumer_sleep_limit = 0;
    const bool debug = false;

    std::vector<pthread_t> consumers;
    std::vector<int> consumers_psum;

    unsigned num_consumers_started = 0;
    pthread_cond_t consumers_ready_cond = PTHREAD_COND_INITIALIZER;
    pthread_mutex_t consumers_ready_mutex = PTHREAD_MUTEX_INITIALIZER;

    int shared_data = 0;
    DataState state = DataState::Empty;
    pthread_mutex_t data_lock = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t empty = PTHREAD_COND_INITIALIZER;
    pthread_cond_t full = PTHREAD_COND_INITIALIZER;
};

pthread_key_t key;

void free_local_data(void* data)
{
    std::cout << "Free data\n";
    free(data);
}

void create_key()
{
    pthread_key_create(&key, free_local_data);
}

int get_tid()
{
    static int current_tid = 1;
    static pthread_mutex_t tid_init_lock = PTHREAD_MUTEX_INITIALIZER;
    static pthread_once_t tid_contr = PTHREAD_ONCE_INIT;
    
    pthread_once(&tid_contr, create_key);
    
    int* tid_ptr = (int*)pthread_getspecific(key);
    if (tid_ptr == nullptr)
    {
        pthread_mutex_lock(&tid_init_lock);
    
        tid_ptr = (int*)malloc(sizeof(int));
        *tid_ptr = current_tid++;

        pthread_setspecific(key, tid_ptr);
        pthread_mutex_unlock(&tid_init_lock);
    }
    
    return *tid_ptr;
}

void* producer_routine(void* arg)
{
    auto data = reinterpret_cast<ThreadData*>(arg);

    pthread_mutex_lock(&data->consumers_ready_mutex);
    while (data->num_consumers != data->num_consumers_started)
        pthread_cond_wait(&data->consumers_ready_cond, &data->consumers_ready_mutex);
    pthread_mutex_unlock(&data->consumers_ready_mutex);

    int value;
    while (std::cin >> value)
    {
        pthread_mutex_lock(&data->data_lock);
        
        data->shared_data = value;
        data->state = DataState::Ready;

        pthread_cond_signal(&data->full);

        while (data->state == DataState::Ready)
            pthread_cond_wait(&data->empty, &data->data_lock);
        
        pthread_mutex_unlock(&data->data_lock);
    }

    // notify all consumer threads about exit
    {
        pthread_mutex_lock(&data->data_lock);

        data->state = DataState::End;

        pthread_cond_broadcast(&data->full);
        pthread_mutex_unlock(&data->data_lock);
    }

    return nullptr;
}
 
void* consumer_routine(void* arg)
{
    signal(SIGTERM, SIG_IGN);

    auto data = reinterpret_cast<ThreadData*>(arg);
    auto sleep_limit = data->consumer_sleep_limit * 1000 /* millisec to microsec */;

    pthread_mutex_lock(&data->consumers_ready_mutex);

    auto consumer_idx = data->num_consumers_started++;
    if (data->num_consumers_started == data->num_consumers)
        pthread_cond_broadcast(&data->consumers_ready_cond);

    pthread_mutex_unlock(&data->consumers_ready_mutex);

    bool continue_consume = true;
    while (continue_consume)
    {
        pthread_mutex_lock(&data->data_lock);

        while (data->state == DataState::Empty)
            pthread_cond_wait(&data->full, &data->data_lock);

        auto value = data->shared_data;

        if (data->state == DataState::Ready)
            data->state = DataState::Empty;
        if (data->state == DataState::End)
        {
            pthread_mutex_unlock(&data->data_lock);
            break;
        }

        pthread_cond_signal(&data->empty);
        pthread_mutex_unlock(&data->data_lock);

        data->consumers_psum[consumer_idx] += value;

        if (data->debug)
        {
            std::stringstream ss;
            ss << "(" << get_tid() << ", " << data->consumers_psum[consumer_idx] << ")\n";
            std::cout << ss.str();
        }

        if (sleep_limit > 0)
            usleep(rand() % sleep_limit);
    }

    pthread_key_delete(key);
    return nullptr;
}

void* consumer_interruptor_routine(void* arg)
{
    auto data = reinterpret_cast<ThreadData*>(arg);

    pthread_mutex_lock(&data->consumers_ready_mutex);
    while (data->num_consumers != data->num_consumers_started)
        pthread_cond_wait(&data->consumers_ready_cond, &data->consumers_ready_mutex);
    pthread_mutex_unlock(&data->consumers_ready_mutex);

    while (data->state != DataState::End)
    {
        auto thread = data->consumers[rand()%data->consumers.size()];
        pthread_kill(thread, SIGTERM);
    }
    
    return nullptr;
}
 
int run_threads(unsigned consumer_cnt, unsigned consumer_sleep_limit, bool debug) 
{
    auto tdata = ThreadData(consumer_cnt, consumer_sleep_limit, debug);

    pthread_t producer;
    pthread_t consumer_interruptor;

    PTHREAD_CHECK(pthread_create(&producer, nullptr, producer_routine, &tdata));
    PTHREAD_CHECK(pthread_create(&consumer_interruptor, nullptr, consumer_interruptor_routine, &tdata));

    for (unsigned i = 0; i < consumer_cnt; i++)
        PTHREAD_CHECK(pthread_create(&tdata.consumers[i], nullptr, consumer_routine, &tdata));

    PTHREAD_CHECK(pthread_join(producer, nullptr));
    PTHREAD_CHECK(pthread_join(consumer_interruptor, nullptr));

    for (unsigned i = 0; i < consumer_cnt; i++)
        PTHREAD_CHECK(pthread_join(tdata.consumers[i], nullptr));

    auto acc = std::accumulate(tdata.consumers_psum.begin(), tdata.consumers_psum.end(), decltype(tdata.consumers_psum)::value_type(0));
    return acc;
}
 
int main(int argc, char** argv) {
    bool debug = false;
    unsigned consumer_cnt;
    unsigned consumer_sleep_limit;

    if (argc < 3)
    {
        std::cerr << "Example: posix consumer_cnt consumer_sleep_limit [-debug]\n"
                  << std::setw(32) <<         "consumer_cnt" << "\t Number of consumers thread \n"
                  << std::setw(32) << "consumer_sleep_limit" << "\t Consumer sleep limit in ms \n"
                  << std::setw(32) <<               "-debug" << "\t Enable debug prints during consumer calculation (Optional) \n";
        return 1;
    }

    consumer_cnt = std::atoi(argv[1]);
    consumer_sleep_limit = std::atoi(argv[2]);

    if (argc >= 4 && !strcmp(argv[3], "-debug"))
        debug = true;

    srand(time(0));

    std::cout << run_threads(consumer_cnt, consumer_sleep_limit, debug) << std::endl;
    return 0;
}
