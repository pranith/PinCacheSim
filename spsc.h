#include <atomic>

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
  int size(void) { return num_elements;}
 private:
  int num_elements;
  q_element<T> *root;
};

template<class T>
LFQueue<T>::LFQueue()
{
  root = new q_element<T>();
  root->next = root;
  root->prev = root;
  num_elements = 0;
}

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

  root->next->prev = node;
  root->next = node;

  if (!num_elements)
    root->prev = node;

  num_elements++;
}

template<class T>
T LFQueue<T>::pop(void)
{
  if (!num_elements)
    return (T)NULL;

  q_element<T> *node = root->prev;
  T ret = node->data;

  root->prev = node->prev;
  root->prev->next = root;
  
  delete node;

  num_elements--;
  return ret;
}
