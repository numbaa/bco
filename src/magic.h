#define __BCO_UNIQUE_FUNC(Module, File, Line) Module##File##Line()
#define BCO_UNIQUE_FUNC(Module) __BCO_UNIQUE_FUNC(Module, __FILE__, __LINE__)