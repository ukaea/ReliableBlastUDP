ifeq ($(strip $(COMPILER)),)
error:
	@echo 'Please specify a c++ compiler by "make COMPILER=<your-choice-of-compuler>"'
	@exit 1
endif

CXFLAGS := -pthread
BINDIR  := ./bin
SRCDIR  := ./src
TARGET  := $(BINDIR)/rbudp

all: $(TARGET)

$(TARGET): 
	@mkdir -p $(BINDIR)
	$(COMPILER) $(CXFLAGS) $(SRCDIR)/main.cpp -o $@

clean:
	$(RM) -r $(BINDIR)
	$(RM) $(SRCDIR)/*.gch

.PHONY: clean
