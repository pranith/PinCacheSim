#include <iostream>
#include <thread>

#include "spsc.h"

bool wait = true, done = false;
LFQueue<int> queue;

void producer()
{
  while(wait);

  for (long j = 0; j < 100000; j++)
  for (long i = 0; i < 100000; i++)
    queue.push(i);

  done = true;
}

void consumer()
{
  while(wait);

  while(!done) {
    if (queue.size())
      queue.pop();
  }
}

int main()
{

  std::thread t1(producer);
  std::thread t2(consumer);

  wait = false;
  
  t1.join();
  t2.join();

  return 0;
}
