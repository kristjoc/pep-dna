APPS_PATH=apps/
SCRIPTS_PATH=scripts/

all:
	$(MAKE) -C $(APPS_PATH)httpapp/ all
	$(MAKE) -C $(APPS_PATH)myhttping/ all
	$(MAKE) -C $(APPS_PATH)tcprdr/ all
	sudo cp $(SCRIPTS_PATH)cpumemscripts/* /usr/bin/
install:
	$(MAKE) -C $(APPS_PATH)httpapp/ install
	$(MAKE) -C $(APPS_PATH)myhttping/ install
	$(MAKE) -C $(APPS_PATH)tcprdr/ install
clean:
	$(MAKE) -C $(APPS_PATH)httpapp/ clean
	$(MAKE) -C $(APPS_PATH)myhttping/ clean
	$(MAKE) -C $(APPS_PATH)tcprdr/ clean
