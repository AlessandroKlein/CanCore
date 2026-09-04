#pragma once

/*
 * Búfer circular ligera (Lock-free Ring Buffer) de un productor y un
 * consumidor (SPSC).
 *
 *   - El productor (una ISR de CAN) hace una unica operacion: copiar la trama
 *     al buffer y avanzar el indice. Se libera la interrupcion en microsegundos.
 *   - El consumidor (el bucle principal) drena las tramas en tick().
 *
 * Reserva una casilla para distinguir lleno de vacio. Los indices son
 * volatiles y la memoria se ordena con una barrera de compilador. En AVR
 * (indices de 8/16 bits) las operaciones de indice son atomicas; en entornos
 * con RTOS el usuario puede inyectar lock/unlock en SystemApi si el
 * productor y el consumidor corren en nucleos distintos.
 */

#include <stddef.h>
#include <stdint.h>

namespace pcd {

template <typename T, size_t Capacity>
class RingBuffer {
  public:
    RingBuffer() : head_(0), tail_(0), count_(0) {}

    bool push(const T &item) {
        if (count_ >= kEffectiveCapacity) {
            return false;  // lleno (se reserva una celda para distinguir vacio de lleno)
        }
        buffer_[head_] = item;
        /* Barrera de compilador: el item queda escrito antes de que avance
         * el indice. En un CPU multinucleo real el usuario debe usar una
         * barrera de memoria de hardware (o lock()) en push/pop. */
        __asm__ __volatile__("" ::: "memory");
        head_ = (head_ + 1) % Capacity;
        ++count_;
        return true;
    }

    bool pop(T &item) {
        if (count_ == 0) {
            return false;  // vacio
        }
        item = buffer_[tail_];
        __asm__ __volatile__("" ::: "memory");
        tail_ = (tail_ + 1) % Capacity;
        --count_;
        return true;
    }

    size_t count() const { return count_; }
    bool empty() const { return count_ == 0; }
    bool full() const { return count_ >= kEffectiveCapacity; }

    /* Descartar todos los elementos pendientes (por ejemplo tras bus-off). */
    void clear() {
        head_ = 0;
        tail_ = 0;
        count_ = 0;
    }

  private:
    static const size_t kEffectiveCapacity = (Capacity > 0) ? (Capacity - 1) : 0;

    volatile size_t head_;
    volatile size_t tail_;
    size_t count_;
    T buffer_[Capacity];
};

}  // namespace pcd
