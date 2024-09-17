#pragma once

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>

#include <utility> // std::pair
template <typename U, typename V>
using my_pair = std::pair<U, V>;

typedef uint64_t  u64;
typedef uint32_t  u32;
typedef uint16_t  u16;
typedef uint8_t   u8;

typedef int64_t   i64;
typedef int32_t   i32;
typedef int16_t   i16;
typedef int8_t    i8;

typedef double    f64;
typedef float     f32;

#include <glm/glm.hpp>
#define Matrix4 glm::mat4
#define Vector2 glm::vec2
#define Vector3 glm::vec3
#define Vector4 glm::vec4

#include <glm/gtx/quaternion.hpp>
#define Quaternion glm::quat

#include <typeindex>
#define cmp_var_type_to_type(one, two) (one == std::type_index(typeid(two)))
#define _type_Type             std::type_index
#define _make_Type(some_type)  std::type_index(typeid(some_type))
#define _set_type_Type(type_var, Target_Type) (type_var = std::type_index(typeid(Target_Type)))

enum class Allocator_Mode
{
    ALLOCATE = 0,
    RESIZE   = 1,
    FREE     = 2,
    FREE_ALL = 3,
    COUNT
};

typedef void*(*Allocator_Proc)(Allocator_Mode mode, i64 size, i64 old_size, void *old_memory, void *allocator_data);
const extern Allocator_Proc __default_allocator;

struct Allocator
{
    void          *data = NULL; // this is the data to allocate from (say, a pool of storage)
    Allocator_Proc proc = NULL;
    // bool           init = false;

    operator bool() { return proc != NULL; }
};

void *heap_allocator(Allocator_Mode mode, i64 size, i64 old_size, void *old_memory, void *allocator_data);

/*
  Temporary storage is a special kind of Allocator, more specifically a simple linear allocator/bump allocator.
  An allocation is a simple increment into a block of memory. Objects can no tbe freed individually.

  The memory of the temporary storage resides in the global_context.

  You can't free individual items in Temporary Storage.
  Also, don't keep pointers to things in Temporary Storage.
 */
constexpr i64 __DEFAULT_TS_SIZE = 40000;
struct Temporary_Storage
{
    u8 *data            = NULL;
    i64 size            = __DEFAULT_TS_SIZE;
    i64 occupied        = 0;
    i64 high_water_mark = 0;
};
extern Temporary_Storage __default_temporary_storage;

struct Level_Set;
struct Context
{
    u32               thread_index; // @Incomplete: No multithreading currently.
    Allocator         allocator;
    Temporary_Storage *temporary_storage;

    Level_Set *current_level_set;
};
extern Context global_context;

#define newline()  printf("\n");

void *my_alloc(i64 size, Allocator allocator = {});
void my_free(void *memory, Allocator allocator = {});

template <typename T>
inline
T *New(bool should_init = true, Allocator allocator = {})
{
    auto result = (T*)(my_alloc(sizeof(T), allocator));
    memset(result, 0, sizeof(T));

    if (should_init)
    {
        T dummy;
        auto dest = reinterpret_cast<u8*>(result);
        auto src  = reinterpret_cast<u8*>(&dummy);
        memcpy(dest, src, sizeof(T));
    }

    return result;
}

#include "array.h"
#include "newstring.h"

#define logprint(agent, ...) __logprint(__FUNCTION__, __LINE__, agent, __VA_ARGS__)
__attribute__ ((format (printf, 4, 5)))
void __logprint(const char *func, long line, const char *agent, const char *fmt, ...);

// At the beginning or at the end of each frame, call this function
// to nuke the temporary storage pool.
void reset_temporary_storage();

// If the high_water_mark exceeds the temporary storage memory capacity, temporary storage will
// default back to the normal heap allocator to allocate more memory.

i64 get_temporary_storage_mark();
void set_temporary_storage_mark(i64 mark);
void log_ts_usage();

void *__temporary_allocator(Allocator_Mode mode, i64 size, i64 old_size, void *old_memory, void *allocator_data);

// Math stuff
#include <cmath>
#define TAU (M_PI * 2)

// Defer macro/thing.
#define MY_CONCAT_INTERNAL(x,y) x##y
#define MY_CONCAT(x,y) MY_CONCAT_INTERNAL(x,y)
 
template<typename T>
struct ExitScope {
    T lambda;
    ExitScope(T lambda):lambda(lambda){}
    ~ExitScope(){lambda();}
    ExitScope(const ExitScope&);
  private:
    ExitScope& operator =(const ExitScope&);
};
 
class ExitScopeHelp {
  public:
    template<typename T>
        ExitScope<T> operator+(T t){ return t;}
};
#define defer const auto& MY_CONCAT(defer__, __LINE__) = ExitScopeHelp() + [&]()

// Math stuff
// Theta here is all in radians unless specified otherwise
f32     lerp(f32 x, f32 y, f32 t);
Vector2 lerp(Vector2 x, Vector2 y, f32 t);
Vector3 lerp(Vector3 x, Vector3 y, f32 t);
Vector4 lerp(Vector4 x, Vector4 y, f32 t);

Quaternion nlerp(Quaternion x, Quaternion y, f32 t);
Quaternion negate(Quaternion q);

Vector2 rotate(Vector2 v,  f32 theta);
f32 normalize_or_zero(Vector3 *dir);

Vector2 unit_vector(Vector2 v);
Vector3 unit_vector(Vector3 v);

void print_cmaj_as_rmaj(Matrix4 mat); // Print column major as row major

void set_rotation(Matrix4 *rotation_matrix, Quaternion orientation);
void get_ori_from_rot(Quaternion *ori, Vector3 axis_of_rot, f32 theta);
void get_rot_mat(Matrix4 *mat, Vector3 axis_of_rot, f32 theta);

Vector3 rotate(Vector3 v, Quaternion ori);

f32 sign_float(f32 x);

f32 move_toward(f32 a, f32 b, f32 amount);
Vector3 move_toward(Vector3 a, Vector3 b, f32 amount);

template <typename T>
void Clamp(T *x, T lower, T higher)
{
    static_assert(std::is_arithmetic<T>::value);

    if (*x < lower)  *x = lower;
    if (*x > higher) *x = higher;
}

