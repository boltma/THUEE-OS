#include <cstring>
#include <ctime>
#include <iostream>
#include <fstream>
#include <pthread.h>
#include <queue>
#include <semaphore.h>
#include <unistd.h>
#include <vector>
using namespace std;

#define _REENTRANT

const int counter_num = 500;

class Customer
{
public:
	pthread_t tid; // thread id
	int id; // customer id
	int counter; // counter id
	int arrival; // arrival time
	int serve; // serve begin time
	int wait; // wait time during serve
	Customer(int id, int arrival, int wait): tid(0), id(id), arrival(arrival), serve(0), wait(wait) {}
};

class Counter
{
public:
	pthread_t tid; // thread id
	int id; // counter id
	Counter(): tid(0), id(0) {}
};

time_t begin_time;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
int customer_num;
sem_t customer_sem;
sem_t counter_sem;
vector<Customer> customer_list;
vector<Counter> counter_list(counter_num);
queue<int> customer_queue; // customers waiting

void* customer_thread(void* c)
{
	Customer &customer = *(Customer*)c;
	sleep(customer.arrival); // simulate arrival time
	pthread_mutex_lock(&lock);
	cout << "customer arrive id " << customer.id << " time " << time(NULL) - begin_time << endl;
	customer_queue.push(customer.id);
	sem_post(&customer_sem);
	pthread_mutex_unlock(&lock);

	sem_wait(&counter_sem); // ready to be served
	pthread_mutex_lock(&lock);
	cout << "customer serve (customer thread) id " << customer.id << " counter id " << customer.counter << " time " << time(NULL) - begin_time << endl;
	pthread_mutex_unlock(&lock);
	sleep(customer.wait); // simulate serve
	pthread_exit(NULL);
	return NULL;
}

void* counter_thread(void* c)
{
	Counter &counter = *(Counter*)c;
	while (true)
	{
		sem_wait(&customer_sem);
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL); // prevent cancellation when counter working
		pthread_mutex_lock(&lock);
		Customer& customer = customer_list[customer_queue.front() - 1];
		customer_queue.pop();
		customer.counter = counter.id;
		customer.serve = time(NULL) - begin_time;
		cout << "customer serve (counter thread) id " << customer.id << " counter id " << customer.counter << " time " << time(NULL) - begin_time << endl;
		pthread_mutex_unlock(&lock);

		sem_post(&counter_sem);
		sleep(customer.wait); // simulate serve
		pthread_mutex_lock(&lock);
		cout << "customer finish id " << customer.id << " time " << time(NULL) - begin_time << endl;
		pthread_mutex_unlock(&lock);
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
		pthread_testcancel(); // maybe unnecessary
	}
}

void read_file()
{
	ifstream input("input.txt", ios::in);
	int id, arrival, wait;
	while (true)
	{		
		input >> id >> arrival >> wait;
		if(input.fail())
			break;
		customer_list.push_back(Customer(id, arrival, wait));
		++customer_num;
	}
}

int main()
{
	pthread_mutex_init(&lock, NULL);
	sem_init(&counter_sem, 0, 0);
	sem_init(&customer_sem, 0, 0);
	read_file();
	begin_time = time(NULL);
	for (int i = 0; i < counter_list.size(); ++i)
	{
		Counter& counter = counter_list[i];
		counter.id = i + 1;
		int error = pthread_create(&(counter.tid), NULL, counter_thread, (void*)(&counter));
		if (error)
		{
			cerr << "Create counter thread " << i + 1 << " failed. " << strerror(error) << endl;
			exit(-1);
		}
	}

	for (int i = 0; i < customer_list.size(); ++i)
	{
		Customer& customer = customer_list[i];
		int error = pthread_create(&(customer.tid), NULL, customer_thread, (void*)(&customer));
		if (error)
		{
			cerr << "Create customer thread " << i + 1 << " failed. " << strerror(error) << endl;
			exit(-1);
		}
	}

	for (auto& customer : customer_list)
		pthread_join(customer.tid, NULL); // wait for all customers to finish

	for (auto& counter : counter_list)
		pthread_cancel(counter.tid);

	for (auto& counter : counter_list)
		pthread_join(counter.tid, NULL); // test that all counter threads are finished

	cout << "Finish" << endl;

	ofstream output("output.txt");
	for (auto& customer : customer_list)
		output << customer.id << ' ' << customer.arrival << ' ' << customer.serve << ' ' << customer.serve + customer.wait << ' ' << customer.counter << endl;

	return 0;
}