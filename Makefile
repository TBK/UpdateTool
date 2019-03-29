NAME = UpdateTool

CXX = g++
CFLAGS = -O3 -I"/cygdrive/c/C++/ModuleScanner/include" -fvisibility=hidden -fvisibility-inlines-hidden
LDFLAGS = -ldl -lpthread /cygdrive/c/C++/ModuleScanner/Linux/ModuleScanner.a ../../Resources/Libs/Linux32/steamclient.a
SRC = $(wildcard *.cpp)
OBJ_DIR = Objs
OBJ = $(addprefix $(OBJ_DIR)/, $(SRC:.cpp=.o))

all: dirs $(NAME)
	
dirs:
	mkdir -p $(OBJ_DIR)

$(NAME): $(OBJ)
	$(CXX) -o $@ $^ $(LDFLAGS)

$(OBJ_DIR)/%.o: %.cpp
	$(CXX) -o $@ -c $< $(CFLAGS)

clean:
	rm -rf $(OBJ_DIR)/*.o

mrproper: clean
	rm -f $(NAME)