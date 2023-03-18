#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <queue>

using namespace std;

const int N = 3; // Number of processes
const int M = 2; // Number of messages per process
const int T = 2; // Number of threads per process


mutex mtx;
condition_variable cv;
vector<queue<pair<int, int>>> buffers(N);
vector<int> counters(N);
vector<vector<int>> vectorClocks(N, vector<int>(N, 0));

void send(int i, int j, int k) {
    unique_lock<mutex> lock(mtx);
    vectorClocks[i][i]++;
    cout << "Process " << i+1 << " sends message " << k << " to process " << j+1 << " with vector clock: [ ";
    for (int l = 0; l < N; l++) {
        cout << vectorClocks[i][l] << " ";
    }
    cout << "]" << endl;
    buffers[j].push(make_pair(i, k));
    cv.notify_all();
}

void receive(int i) {
    unique_lock<mutex> lock(mtx);
    while (counters[i] < M) {
        cv.wait(lock, [i] { return !buffers[i].empty(); });
        pair<int, int> msg = buffers[i].front();
        buffers[i].pop();
        vectorClocks[i][i] = max(vectorClocks[i][i], vectorClocks[msg.first][i]);
        for (int j = 0; j < N; j++) {
            vectorClocks[i][j] = max(vectorClocks[i][j], vectorClocks[msg.first][j]);
        }
        if (msg.second == counters[msg.first]) {
            cout << "Process " << i+1 << " received message " << msg.second << " from process " << msg.first+1
                 << " (VC: [";
            for (int j = 0; j < N; j++) {
                cout << vectorClocks[i][j];
                if (j < N - 1) {
                    cout << ", ";
                }
            }
            cout << "])" << endl;
            counters[msg.first]++;
        } else {
            cout << "Process " << i+1 << " received message " << msg.second << " from process " << msg.first+1
                 << " (buffered, VC: [";
            for (int j = 0; j < N; j++) {
                cout << vectorClocks[i][j];
                if (j < N - 1) {
                    cout << ", ";
                }
            }
            cout << "])" << endl;
            cout << "Process " << i+1 << " is waiting for message " << counters[msg.first] << " from process " << msg.first+1
                 << " (VC: [";
            for (int j = 0; j < N; j++) {
                cout << vectorClocks[i][j];
                if (j < N - 1) {
                    cout << ", ";
                }
            }
            cout << "])" << endl;
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
                thread(send, i, j, k).detach();
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
