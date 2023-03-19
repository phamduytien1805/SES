#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <stack>
#include <deque>
#include <random>
#include <chrono>
#include <fstream>
#include <string>

using namespace std;

struct Message
{
    int processID;
    vector<int> timestamp;
    vector<vector<int>> vector;
};

const int N = 3; // Number of processes
const int M = 2; // Number of messages per process
const int T = 2; // Number of threads per process

mutex mtx;
vector<mutex> fileMutexes(N);

condition_variable cv;
vector<deque<Message>> msgQueue(N);
vector<deque<Message>> buffers(N);
vector<vector<vector<int>>> vector_P(N, vector<vector<int>>(N, vector<int>(N, 0)));
vector<vector<int>> vectorClocks(N, vector<int>(N, 0));

void printLocalTime(vector<int> &vectorClock)
{
    cout << "[ ";
    for (int l = 0; l < N; l++)
    {
        cout << vectorClock[l] << " ";
    }
    cout << "] " << endl;
}
void printCachedVector(vector<vector<int>> &v)
{
    cout << "[ ";
    for (int i = 0; i < v.size(); i++)
    {
        if (i > 0)
        {
            cout << ",";
        }
        cout << " VP_" << i << ": (";
        for (int j = 0; j < v[i].size(); j++)
        {
            cout << v[i][j];
            if (j < v[i].size() - 1)
            {
                cout << " , ";
            }
        }
        cout << ") ";
    }
    cout << " ]";
}
void receivedInfo(int processID, vector<int> localTime, vector<vector<int>> currentCachedVectors, Message &msg, bool isBuffered = false, bool isFromBuffered = false)
{
    cout << "##################" << endl;
    cout << "Received:" << endl;
    if (isBuffered)
    {
        cout << "(Buffered message): ";
    }
    else if (isFromBuffered)
    {
        cout << "(Message From Buffer): ";
    }
    else
    {
        cout << "(Message): ";
    }
    cout << "P" << processID << " <-- P" << msg.processID << endl;
    cout << "Message timestamp: ";
    printLocalTime(msg.timestamp);
    if (!isBuffered)
    {
        cout << "\nUpdated V_P" << processID << ": ";
        printCachedVector(currentCachedVectors);
    }
    cout << endl;
    cout << "P" << processID << " received timestamp: ";
    printLocalTime(localTime);
    cout << endl;
    cout << "##################" << endl;
}
void sendedInfo(int &sourcePID, int &destPID, Message &msg)
{
    cout << "##################" << endl;
    cout << "Sended:" << endl;
    cout << "P" << sourcePID << " -> P" << destPID;
    cout << " local time: ";
    printLocalTime(msg.timestamp);
    cout << " V_P" << sourcePID << ": ";
    printCachedVector(msg.vector);
    cout << endl;
    cout << "##################" << endl;
}
bool isLessThanOrEqual(int processId, vector<int> vectorInMsg, vector<int> localVectorClock)
{
    // Check that the input vectors have the same size
    if (vectorInMsg.size() != localVectorClock.size())
    {
        cerr << "Error: Oops something went wrong with size of vector!";
        exit(EXIT_FAILURE);
    }

    // Check that each element in vectorInMsg is less than or equal to the corresponding
    // element in localVectorClock, except for the element at index.
    for (size_t i = 0; i < vectorInMsg.size(); i++)
    {
        if (i == processId)
        {
            // Skip the element at current processID
            continue;
        }
        if (vectorInMsg[i] > localVectorClock[i])
        {
            return false;
        }
    }

    return true;
}

vector<int> mergeVector(const vector<int> &a, const vector<int> &b)
{
    // Create a new vector to store the merged values
    std::vector<int> merged(a.size());

    // Merge the values by taking the maximum value at each position
    for (size_t i = 0; i < a.size(); i++)
    {
        merged[i] = max(a[i], b[i]);
    }

    return merged;
}
void send(int sourcePID, int destPID)
{
    unique_lock<mutex> lock(mtx);
    // unique_lock<mutex> lock(fileMutexes[sourcePID]);
    ofstream file(to_string(sourcePID) + ".txt", ios_base::app);

    vectorClocks[sourcePID][sourcePID]++;
    Message msg = {sourcePID, vectorClocks[sourcePID], vector_P[sourcePID]};

    static thread_local std::mt19937 rng(std::random_device{}());
    if (std::bernoulli_distribution{0.4}(rng) && !msgQueue[destPID].empty())
    {
        msgQueue[destPID].push_front(msg);
    }
    else
    {
        msgQueue[destPID].push_back(msg);
    }
    // msgQueue[destPID].push(msg);
    sendedInfo(sourcePID, destPID, msg);
    vector_P[sourcePID][destPID][sourcePID]++;
    // vector<vector<int>> cachedVector;
    cv.notify_all();
}

void receive(int receiverPID)
{
    unique_lock<mutex> lock(mtx);
    while (true)
    {
        cv.wait(lock, [receiverPID]
                { return !msgQueue[receiverPID].empty(); });
        Message msg = msgQueue[receiverPID].front();

        if (!isLessThanOrEqual(receiverPID, msg.vector[receiverPID], vectorClocks[receiverPID]))
        {
            buffers[receiverPID].push_back(msg);
            msgQueue[receiverPID].pop_front();
            receivedInfo(receiverPID, vectorClocks[receiverPID], {{}}, msg, true);
            break;
        }
        vectorClocks[receiverPID][receiverPID]++;
        auto senderClock = msg.timestamp;
        auto senderVector_P = msg.vector;

        for (int j = 0; j < N; j++)
        {
            if (j != receiverPID)
            {
                vector_P[receiverPID][j] = mergeVector(vector_P[receiverPID][j], senderVector_P[j]);
            }
            vectorClocks[receiverPID][j] = max(vectorClocks[receiverPID][j], senderClock[j]);
        }
        receivedInfo(receiverPID, vectorClocks[receiverPID], vector_P[receiverPID], msg);
        msgQueue[receiverPID].pop_front();
        while (!buffers[receiverPID].empty())
        {
            Message msgBuffered = buffers[receiverPID].front();

            if (!isLessThanOrEqual(receiverPID, msgBuffered.vector[receiverPID], vectorClocks[receiverPID]))
            {
                break;
            }
            vectorClocks[receiverPID][receiverPID]++;
            auto senderBufferedClock = msgBuffered.timestamp;
            auto senderBufferedVector_P = msgBuffered.vector;
            for (int j = 0; j < N; j++)
            {
                vector_P[receiverPID][j] = mergeVector(vector_P[receiverPID][j], senderBufferedVector_P[j]);
                vectorClocks[receiverPID][j] = max(vectorClocks[receiverPID][j], senderBufferedClock[j]);
            }
            receivedInfo(receiverPID, vectorClocks[receiverPID], vector_P[receiverPID], msg, false, true);
            buffers[receiverPID].pop_front();
        }
    }
}
void process(int i)
{
    vector<thread> threads;
    for (int j = 0; j < T; j++)
    {
        threads.push_back(thread(receive, i));
    }
    for (int j = 0; j < N; j++)
    {
        if (j != i)
        {
            for (int k = 0; k < M; k++)
            {
                thread(send, i, j).detach();
            }
        }
    }
    for (int j = 0; j < T; j++)
    {
        threads[j].join();
    }
}

int main()
{
    vector<thread> processes;
    for (int i = 0; i < N; i++)
    {
        processes.push_back(thread(process, i));
    }
    for (int i = 0; i < N; i++)
    {
        processes[i].join();
    }
    return 0;
}
