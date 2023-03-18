#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <queue>

using namespace std;

const int N = 3; // Number of processes
const int M = 3; // Number of messages per process
const int T = 2; // Number of threads per process

mutex mtx;
condition_variable cv;
vector<queue<pair<int, int>>> buffers(N);
vector<int> counters(N);

void send(int i, int j, int k) {
    unique_lock<mutex> lock(mtx);
    buffers[j].push(make_pair(i, k));
    cv.notify_all();
}

void receive(int i) {
    unique_lock<mutex> lock(mtx);
    while (counters[i] < M) {
        cv.wait(lock, [i] { return !buffers[i].empty(); });
        pair<int, int> msg = buffers[i].front();
        buffers[i].pop();
        if (msg.second == counters[msg.first]) {
            cout << "Process " << i << " received message " << msg.second << " from process " << msg.first << endl;
            counters[msg.first]++;
        } else {
            cout << "Process " << i << " received message " << msg.second << " from process " << msg.first << " (buffered)" << endl;
            cout << "Process " << i << " is waiting for message " << counters[msg.first] << " from process " << msg.first << endl;
        }
    }
}

void process(int i) {
    vector<thread> threads;
    for (int j = 0; j < T; j++) {
        threads.push_back(thread(receive, i));
    }
    for (int j = 0; j < N; j++) {
        if (j != i) {
            for (int k = 0; k < M; k++) {
                send(i, j, k);
            }
        }
    }
    for (int j = 0; j < T; j++) {
        threads[j].join();
    }
}

int main() {
    vector<thread> processes;
    for (int i = 0; i < N; i++) {
        processes.push_back(thread(process, i));
    }
    for (int i = 0; i < N; i++) {
        processes[i].join();
    }
    return 0;
}
