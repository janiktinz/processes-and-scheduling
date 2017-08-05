// Vierter Schritt - Simulationsexperimente
// Compile: g++ -std=c++11 Simulationsexperimente.cpp -o Simulator

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <list>

using namespace std;

pid_t pid;                          // Prozess ID
pid_t rpid;                         // Reporter Prozess ID
int fd[2];                          // Pipe (Filedeskriptor)
char entry[30];                     // Eingabe
char buf[30];
float current_time = 0;             // Gesamtlaufzeit
int process_counter = 0;
const int SLEEP_TIME = 1;           // Schlafzeit im Automatic Mode

class Process
{
private:
    static int next_id;
    
public:
    int pid = 0;
    int ppid;
    int priority = 0;
    string file;
    vector<string> memory;          // Befehlsspeicher
    
    int value;                      // Integerregister
    int pc;                         // Programmzähler
    
    int time_cpu;                   // Anzahl der CPU Takte
    int round_robin_time;         // Zeit des Prozesses in der CPU
    
    Process()
    {
        pid = ++next_id;
    }
    
    void print()
    {
        cout << pid << "\t" << ppid << "\t" << priority << "\t\t" << value << "\t" << time_cpu*SLEEP_TIME << "\t" << time_cpu << endl;
    }
};

int Process::next_id = 0;

Process* current = NULL;
list<Process*> blocked;
list<Process*> waiting;

Process* create(string file)
{
    Process* process = new Process();
    process->file = file;
    process->value = 0;
    process->pc = 0;
    process->time_cpu = 0;
    process->round_robin_time = 0;
    
    if (current != NULL)
    {
        process->ppid = current->pid;
    } else
    {
        process->ppid = 0;          // Kein Unterprozess
    }
    
    // Lesen von der Datei
    ifstream input(file);
    for (string line; getline(input, line);)
    {
        process->memory.push_back(line);  // Speichere Prozess in Speicher Vektor
    }
    process_counter++;
    return process;
}

void next_process()   // Nächster Prozess in die CPU holen
{
    if (waiting.empty())
    {
        current = NULL;
    }
    else
    {
        current = waiting.back();
        waiting.pop_back();
    }
}

void unblock()   // Nächster Prozess entblockieren
{
    if (current == NULL)
    {
        waiting.push_front(blocked.back());
        blocked.pop_back();
        next_process();
        return;
    }
    if (blocked.empty())
    {
        next_process();
    }
    else
    {
        waiting.push_front(blocked.back());
        blocked.pop_back();
        
        waiting.push_front(current);
        next_process();
    }
}

void remove()   // Simulierten Prozess beenden
{
    cout << "Execution of pid " << current->pid << " completed" << endl;
    
    delete current;
    current = NULL;
    if (!waiting.empty())
        next_process();
    /*else if(blocked.empty() && waiting.empty())
     {
     return;
     }
     else if(!blocked.empty())
     {
     waiting.push_front(blocked.back());   // Automatisches Entblockieren von Prozessen ohne "U" Kommandant-Befehl
     blocked.pop_back();
     next_process();
     }*/
}

void step()
{
    stringstream line(current->memory[current->pc++]);
    current->time_cpu++;
    current_time++;
    
    char cmd;
    line >> cmd;
    
    switch (cmd)
    {
        case 'S':
        {
            line >> current->value;
            cout << "> " << cmd << " " << current->value <<endl;
            break;
        }
        case 'A':
        {
            int value_a;
            line >> value_a;
            cout << "> " << cmd << " " << value_a << endl;
            current->value = current->value + value_a;
            break;
        }
        case 'D':
        {
            int value_b;
            line >> value_b;
            cout << "> " << cmd << " " << value_b << endl;
            current->value = current->value - value_b;
            break;
        }
        case 'B':
        {
            cout << "> " << cmd << endl;
            if(blocked.empty() &&  waiting.empty())
                break;
            else
            {
                current->round_robin_time = 0;
                blocked.push_front(current);
                if(waiting.empty())
                {
                    current = NULL;
                    break;
                }
                next_process();
            }
            return;
        }
        case 'E':
        {
            cout << "> " << cmd << endl;
            remove();
            return;
        }
        case 'R':
        {
            cout << "> " << cmd;
            string file;
            line >> file;
            cout << " " << file << endl;
            Process* process = create(file);
            waiting.push_front(process);
            break;
        }
    }
    
    // Für Round-Robin-Scheduling
    current->round_robin_time++;
    if (current->round_robin_time > 2)
    {
        if(blocked.empty() && waiting.empty())
            current->round_robin_time = 0;
        else
        {
            current->round_robin_time = 0;
            waiting.push_front(current);
            next_process();
        }
    }
}

void init(string file)   // init Prozess
{
    Process* newProcess = create(file);
    current = newProcess;
}

void print()   // Ausgabe des aktuellen Zustands des Systems mittels Reporterprozess
{
    cout << "*********************************************************************" << endl;
    cout << "The current system state is as follows:" << endl;
    cout << "*********************************************************************" << endl;
    cout << "CURRENT TIME: " << current_time <<endl;
    cout << "RUNNING PROCESS:" << endl;
    cout << "pid\t" << "ppid\t" << "priority\t" << "value\t" << "time\t" << "CPU time used so far" << endl;
    if (current != NULL)
        current->print();
    
    cout << "BLOCKED PROCESSES:" << endl;
    cout << "pid\t" << "ppid\t" << "priority\t" << "value\t" << "time\t" << "CPU time used so far" << endl;
    
    list<Process*>::iterator iter;
    for (iter = blocked.begin(); iter != blocked.end(); iter++)
    {
        (*iter)->print();
    }
    
    cout << "PROCESSES READY TO EXECUTE:" << endl;
    cout << "pid\t" << "ppid\t" << "priority\t" << "value\t" << "time\t" << "CPU time used so far" << endl;
    
    for (list<Process*>::iterator iter = waiting.begin(); iter != waiting.end(); iter++)
    {
        (*iter)->print();
    }
    
    cout << "*********************************************************************" << endl;
}

void commanderProcess()
{
    int status;
    char entry[30];
    while (1)
    {
        gets(entry);
        write(fd[1], entry, sizeof(entry)/sizeof(char));
        
        if (*entry == 'Q')
            break;
    }
    wait(&status);
    printf("Exit Kommandant, PID: %d\n", getpid());
    exit(0);
}

void reporterProcess()
{
    printf("I am a reporter process, PID: %d\n", getpid());
    
    print();
    exit(0);
}

void processManagerProcess()
{
    init("init");
    int status;
    while (1)
    {
        // Filedeskriptor zum Schreiben in die Pipe schließen
        close(fd[1]);
        read(fd[0], buf, sizeof(buf)/sizeof(char));   // Filedeskriptor zum Lesen aus der Pipe
        
        if (*buf == 'P') // Falls Eingabe 'P'
        {
            if((rpid = fork()) == -1) // Kopie/ Reporter erzeugen
            {
                perror("fork() error");
                exit(1);
            } else if(rpid == 0)        // Reporter Prozess
            {
                reporterProcess();
            }
            else                        // Prozessmanager wartet auf Reporter
            {
                wait(&status);
                printf("Reporter process finished, PID: %d\n", rpid);
            }
        } else if (*buf == 'Q') // Falls Einagbe 'Q'
        {
            cout << "AVERAGE TURN AROUND TIME: " << (current_time)/(process_counter) << endl;
            printf("Exit Prozessmanager, PID: %d\n", getpid());
            exit(0);
        } else if (*buf == 'M')
        {
            while (1)
            {
                if (current != NULL)
                {
                    step();
                    sleep(SLEEP_TIME);
                }
                if (waiting.empty() && blocked.empty() && current == NULL)
                {
                    cout << "All processes completed" << endl;
                    break;
                }
                if(current == NULL)
                    break;
            }
        } else if (*buf == 'S')
        {
            if (current != NULL)
                step();
            if (waiting.empty() && blocked.empty() && current == NULL)
            {
                cout << "All processes completed" << endl;
            }
        } else if (*buf == 'U')
        {
            unblock();
        }
        else
        {
            printf("Invalid input: %s\n", buf);
        }
    }
}

int main(int argc, char *argv[])
{
    //Erstellen einer Pipe
    if ((pipe(fd)) == -1)
    {
        perror("pipe error");
        exit(1);
        // Kopie/ Prozessmanager erzeugen
    } else if ((pid = fork()) == -1)
    {
        perror("fork error");
        exit(1);
    } else if (pid > 0)
    {
        close(fd[0]);
        commanderProcess();
    }
    else
    {
        close(fd[1]);
        processManagerProcess();
    }
    return 0;
}

