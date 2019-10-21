#https://stackoverflow.com/questions/5178125/how-to-place-object-files-in-separate-subdirectory

#Compiler and Linker
CC          := gcc

#The Target Binary Program
TARGET      := cbf_sensor_dashboard

#The Directories, Source, Includes, Objects, Binary and Resources
SRCDIR      := src
INCDIR      := inc
BUILDDIR    := obj
TARGETDIR   := bin
CONFDIR     := conf
HTMLDIR     := html
INSTALLDIR  := /usr/local/sbin
CONFDESTDIR := /etc/cbf_sensor_dashboard
HTMLDESTDIR := /usr/local/share/cbf_sensor_dashboard/html
SYSTEMDDIR  := /lib/systemd/system
SRCEXT      := c
DEPEXT      := d
OBJEXT      := o

#Flags, Libraries and Includes
CFLAGS      := -Wall -Wconversion -ggdb -rdynamic
KATCPDIR    := ../katcp_devel/katcp
LIB         := -L $(KATCPDIR) -lkatcp
INC         := -I$(INCDIR) -I/usr/local/include -I $(KATCPDIR)
INCDEP      := -I$(INCDIR) -I../katcp_devel/katcp

#---------------------------------------------------------------------------------
#DO NOT EDIT BELOW THIS LINE
#---------------------------------------------------------------------------------
SOURCES     := $(shell find $(SRCDIR) -type f -name *.$(SRCEXT))
OBJECTS     := $(patsubst $(SRCDIR)/%,$(BUILDDIR)/%,$(SOURCES:.$(SRCEXT)=.$(OBJEXT)))

#Defauilt Make
all: directories $(TARGET)

#Remake
remake: clean all

#Make the Directories
directories:
	@mkdir -p $(TARGETDIR)
	@mkdir -p $(BUILDDIR)

#Full Clean, Objects and Binaries
clean:
	$(RM) -rf $(BUILDDIR)
	$(RM) -rf $(TARGETDIR)

#Pull in dependency info for *existing* .o files
-include $(OBJECTS:.$(OBJEXT)=.$(DEPEXT))

install: $(TARGETDIR)/$(TARGET)
	install -v -o root -g root -m 744 -C $(TARGETDIR)/$(TARGET) -t $(INSTALLDIR)
	install -v -o root -g root -m 644 -C $(CONFDIR)/30-cbf_sensor_dashboard.conf -t /etc/rsyslog.d
	install -v -o root -g root -m 644 -C -D $(CONFDIR)/cmc_list.conf -t $(CONFDESTDIR)
	install -v -o root -g root -m 644 -C -D $(CONFDIR)/sensor_list.conf -t $(CONFDESTDIR)
	install -v -o root -g root -m 644 -C -D $(HTMLDIR)/* -t $(HTMLDESTDIR)
	install -v -o root -g root -m 644 -C -D $(CONFDIR)/cbf_sensor_dashboard.service -t $(SYSTEMDDIR)
	systemctl enable cbf_sensor_dashboard
	systemctl start cbf_sensor_dashboard

#Link
$(TARGET): $(OBJECTS)
	$(CC) -o $(TARGETDIR)/$(TARGET) $^ $(LIB)

#Compile
$(BUILDDIR)/%.$(OBJEXT): $(SRCDIR)/%.$(SRCEXT)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INC) -c -o $@ $<
	@$(CC) $(CFLAGS) $(INCDEP) -MM $(SRCDIR)/$*.$(SRCEXT) > $(BUILDDIR)/$*.$(DEPEXT)
	@cp -f $(BUILDDIR)/$*.$(DEPEXT) $(BUILDDIR)/$*.$(DEPEXT).tmp
	@sed -e 's|.*:|$(BUILDDIR)/$*.$(OBJEXT):|' < $(BUILDDIR)/$*.$(DEPEXT).tmp > $(BUILDDIR)/$*.$(DEPEXT)
	@sed -e 's/.*://' -e 's/\\$$//' < $(BUILDDIR)/$*.$(DEPEXT).tmp | fmt -1 | sed -e 's/^ *//' -e 's/$$/:/' >> $(BUILDDIR)/$*.$(DEPEXT)
	@rm -f $(BUILDDIR)/$*.$(DEPEXT).tmp

#Non-File Targets
.PHONY: all remake clean cleaner resources
