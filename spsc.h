#include <atomic>
#include <assert.h>
#include <mutex>

template<class T>
class q_element {
public:
  q_element(const T& val)
  {
    data = val;
    next = nullptr;
    prev = nullptr;
  }
  q_element()
  {
    next = nullptr;
    prev = nullptr;
  }
  q_element *next, *prev;
  T data;
};

template<class T>
class LFQueue {
 public:
  LFQueue();
  ~LFQueue();
  void push(const T& el);
  T pop(void);
  int size(void) { return num_elements.load();}
 private:
  std::atomic<int> num_elements;
  q_element<T> *root;
  std::mutex mtx;
};

template<class T>
LFQueue<T>::LFQueue()
{
  root = new q_element<T>();
  root->next = root;
  root->prev = root;
  num_elements = 0;
}

long elements_popped = 0;

template<class T>
LFQueue<T>::~LFQueue()
{
  while(num_elements) {
    pop();
  }
}

template<class T>
void LFQueue<T>::push(const T& el)
{
  q_element<T> *node = new q_element<T>(el);
  node->next = root->next;
  node->prev = root;

  mtx.lock();
  root->next->prev = node;
  root->next = node;
  mtx.unlock();

  num_elements++;
}

template<class T>
T LFQueue<T>::pop(void)
{
  int elnum = num_elements.load();
  assert(elnum != 0);

  q_element<T> *node = root->prev;
  assert(node != root);

  T ret = node->data;

  mtx.lock();
  root->prev = node->prev;
  root->prev->next = root;
  mtx.unlock();
  
  delete node;

  elements_popped++;
  std::atomic_thread_fence(std::memory_order_seq_cst);
  num_elements--;

  assert(root->next != NULL);
  assert(root->prev != NULL);
  return ret;
}
