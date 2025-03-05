#ifndef BUFFER_H_INCLUDED
#define BUFFER_H_INCLUDED

class Buffer
{
  public:
    Buffer();
    ~Buffer();

  private:
    Buffer(Buffer const&) = delete;
    void operator=(Buffer const&) = delete;
};

#endif