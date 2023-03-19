#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <stack>
#include <queue>

using namespace std;

struct Message
{
    int processID;
    vector<int> timestamp;
    vector<vector<int>> vector;
};

const int N = 2; // Number of processes
const int M = 2; // Number of messages per process
const int T = 2; // Number of threads per process

mutex mtx;
condition_variable cv;
vector<queue<Message>> buffers(N);
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
void receivedInfo(int processID, vector<int> localTime, Message msg, bool isBuffered = false)
{
    if (isBuffered)
    {
        cout << "(Buffered message): ";
    }
    else
    {
        cout << "(Message): ";
    }
    cout << "P" << processID << " <-- P" << msg.processID;
    cout << "msg timestamp: ";
    printLocalTime(msg.timestamp);
    cout << " V_P" << msg.processID << ": ";
    printCachedVector(msg.vector);
    cout << endl;
}
void sendedInfo(int &sourcePID, int &destPID, Message &msg)
{
    cout << "P" << sourcePID << " -> P" << destPID;
    cout << " local time: ";
    printLocalTime(msg.timestamp);
    cout << " V_P" << sourcePID << ": ";
    printCachedVector(msg.vector);
    cout << endl;
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
    vectorClocks[sourcePID][sourcePID]++;
    Message msg = {sourcePID, vectorClocks[sourcePID], vector_P[sourcePID]};
    buffers[destPID].push(msg);
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
                { return !buffers[receiverPID].empty(); });
        Message msg = buffers[receiverPID].front();

        if (!isLessThanOrEqual(receiverPID, msg.vector[receiverPID], vectorClocks[receiverPID]))
        {
            receivedInfo(receiverPID, vectorClocks[receiverPID], msg, true);
            break;
        }
        vectorClocks[receiverPID][receiverPID]++;
        auto senderClock = msg.timestamp;
        auto senderVector_P = msg.vector;

        for (int j = 0; j < N; j++)
        {
            mergeVector(vector_P[receiverPID][j], senderVector_P[j]);
            vectorClocks[receiverPID][j] = max(vectorClocks[receiverPID][j], senderClock[j]);
        }
        // vectorClocks[receiverPID][msg.processID] = max(vectorClocks[receiverPID][msg.processID], vectorClocks[msg.processID][msg.processID]);
        receivedInfo(receiverPID, vectorClocks[receiverPID], msg);
        buffers[receiverPID].pop();
        // vectorClocks[i][i] = max(vectorClocks[i][i], vectorClocks[msg.first][i]);
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
