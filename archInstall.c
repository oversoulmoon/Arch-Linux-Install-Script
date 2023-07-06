#include <stdio.h>
#include <unistd.h>  
#include <stdlib.h>
#include <string.h>
#include <errno.h>


FILE *run(char*);
void printFile(FILE *);
void postConfig();
void runInChroot(char *, char *);
int getInteger(const char*,int*);
void checkBIOS(); 
void getDisk();
int checkDiskValid();
void setTimezone();
void printMessage();
int checkNetwork();
void partitionDisk();
void formatDisk();
void mountPartitions();
void installArch();

int UEFI;
char timezone[100];
char device[100];
char name[100];

#define error 0
#define success 1
#define warning 2

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_RESET   "\x1b[0m"

int main(){
	system("clear");
        int has_internet = checkNetwork();
	while(!has_internet){
		printMessage(3,"Please make sure you have a network connection");
		sleep(3);
		has_internet = checkNetwork();
	}
	getDisk();
	partitionDisk();
	formatDisk();
	checkBIOS();
        setTimezone();
	mountPartitions();
	installArch();
	postConfig();
}
void installArch(){
	char line[1024];
	FILE *cpu = run("cat /proc/cpuinfo | grep \"vendor_id\" | head -n 1");
	char packages[1024]="base linux linux-firmware networkmanager base-devel git vim man-db man-pages texinfo grub efibootmgr openssh";

	printMessage(1,"Finding CPU info\n");
	fgets(line, sizeof(line),cpu); 
	pclose(cpu);

	if(strstr(line,"AMD")){
		strcat(packages, " amd-ucode");
	}else if(strstr(line,"Intel")){
		strcat(packages, " intel-ucode");
	}
	printMessage(1, "Determined packages \n");
	snprintf(line, sizeof(line), "pacstrap -K /mnt %s", packages);
	printMessage(2, "Installing packages, this may take a while!\n");
	system(line);
		
}
void postConfig(){
	char line[1024];
	char chrootPath[5] = "/mnt";
	system("genfstab -U /mnt >> /mnt/etc/fstab");
	printMessage(1, "Generated drive mount table\n");

	snprintf(line, sizeof(line), "ln -sf /usr/share/zoneinfo/%s /etc/localtime && hwclock --systohc", timezone);
	runInChroot(chrootPath,line);
	printMessage(1, "Set timezone and synchronized clock\n");

	snprintf(line, sizeof(line), "echo \"en_US.UTF-8 UTF-8\" >> /etc/locale.gen && locale-gen && touch /etc/locale.conf && echo \"LANG=en_US.UTF-8\" >> /etc/locale.conf && exit");
	runInChroot(chrootPath,line);
	printMessage(1, "Configured locales\n");

	snprintf(line, sizeof(line), "echo \"LinuxSystem\" >> /etc/hostname");
	runInChroot(chrootPath,line);
	printMessage(1, "Configured host name\n");

	snprintf(line, sizeof(line), "echo \"root:root\" | chpasswd");
	runInChroot(chrootPath,line);
	printMessage(1, "Changed root password to: root\n");


	snprintf(line, sizeof(line), "systemctl enable NetworkManager && systemctl enable sshd && echo \"PermitRootLogin yes\" >> /etc/ssh/sshd_config");
	runInChroot(chrootPath,line);
	printMessage(1, "Enabled services\n");
	
	if(UEFI){
		snprintf(line, sizeof(line), "grub-install --target=x86_64-efi --efi-directory=/boot --bootloader-id=GRUB");
	}else{
		snprintf(line, sizeof(line), "grub-install --target=i386-pc %s",device);
	}
	runInChroot(chrootPath,line);
	snprintf(line, sizeof(line), "grub-mkconfig -o /boot/grub/grub.cfg && exit");
	runInChroot(chrootPath,line);
	printMessage(1, "Configured boot loader\n");

	printMessage(2, "Installation done, restarting system\n");
	system("reboot");
}

void runInChroot(char * path, char * command){
	char line[1024];
	snprintf(line,sizeof(line), "arch-chroot %s /bin/bash -c \"%s\"",path, command);
	printf("%s\n", command);
	system(line);
}
void mountPartitions(){
	char line[1024];
	snprintf(line, sizeof(line),"mount %s3 /mnt &> /dev/null",device);
	system(line);
	snprintf(line, sizeof(line),"swapon %s2 &> /dev/null",device);
	system(line);
	if(UEFI){
		snprintf(line, sizeof(line),"mount --mkdir %s1 /mnt/boot &> /dev/null",device);
		system(line);
	}
	printMessage(1,"Mounted disk\n");
}
void formatDisk(){
	char line[1024];
	snprintf(line, sizeof(line),"mkfs.ext4 -F %s3 &> /dev/null",device);	
	system(line);
	snprintf(line, sizeof(line),"mkswap %s2 &> /dev/null",device);	
	system(line);
	if(UEFI){
		snprintf(line, sizeof(line),"mkfs.fat -F 32 %s1 &> /dev/null",device);	
		system(line);
	}
	printMessage(1,"Formatted disk \n");
}
void partitionDisk(){
	char line[1024];
	//from here all size are in MiB
	signed int swapSize = 512;
	snprintf(line,sizeof(line),"umount %s?* &> /dev/null",device);
	system(line);
	system("swapoff -a");
	do{
		printMessage(2, "The default swap size is 512 MiB, do you wish to change this? (y/n):");
		scanf("%s",line);
	}while(strcmp(line,"n") != 0 && strcmp(line,"y") != 0 );

	if(strcmp(line, "y")== 0){
		getInteger("test",&swapSize);
	}

	if(UEFI){
		system("echo \"label: gpt\" > /tmp/partitionLayout");
		system("echo \"1: size= 614400\" >> /tmp/partitionLayout");
	}else{
		system("echo \"label:dos\" > /tmp/partitionLayout");
	}
	snprintf(line, sizeof(line),"echo \"2: size= %d\" >> /tmp/partitionLayout",swapSize *2048);	
	system(line);
	system("echo \"3: size= \" >> /tmp/partitionLayout");
	snprintf(line, sizeof(line),"sfdisk %s < /tmp/partitionLayout &>/dev/null",device);	
	system(line);
	printMessage(1,"Partitioned disk\n");
}
int getInteger(const char *prompt, int *i) {
	char line[1024];
	int c;
        while ((c = getchar()) != EOF && c != '\n')
            continue;
	int Invalid = 0;
	int EndIndex;
	char buffer[100];
	do {
		Invalid = 1;
		printMessage(2,"Please enter the size of the SWAP partition in MiB (without units): ");
		if (NULL == fgets(buffer, sizeof(buffer), stdin))
			return 1;
		errno = 0;
	} while ((1 != sscanf(buffer, "%d %n", i, &EndIndex)) || buffer[EndIndex] || errno);
	return 0;
}
void printMessage(int type, char *message){
	char line[1024];
	switch(type){
		case 0:
			printf("\[ " ANSI_COLOR_RED "ERROR" ANSI_COLOR_RESET " ] ");
			printf("%s",message);
			break;
		case 1:
			printf("\[ " ANSI_COLOR_GREEN "DONE" ANSI_COLOR_RESET " ] ");
			printf("%s",message);
			break;
		case 2:
			printf("\[ " ANSI_COLOR_YELLOW "WARNING" ANSI_COLOR_RESET " ] ");
			printf("%s",message);
			break;
		default:
			printf("%s",message);
			break;
	}

}

void getDisk(){
	char line[1024];
	int validDisk;
	do{
		char selected[10];
		printMessage(2,"The minimum requirement of this script is 4GB of storage, listed drives with 4Gb or more.\n");
		printf("\n\nDISKS\n");
		system("lsblk -lSnbo 'NAME',\'SIZE\',\'MOUNTPOINTS\' | awk \'$2>=4294967296\'  | awk \'!$3\' | numfmt --to=iec --field=2");
		printf("\n\n");
		printMessage(2,"Please select the drive you wish to install arch to (THE DISK WILL BE WIPED!): "); 
		scanf("%s",selected);
		//removing \n from scanf return
		strncpy(selected, selected, strlen(selected)-1);
		validDisk = checkDiskValid(selected);
		if(!validDisk){
			printMessage(0, "Not a valid disk\n");
		}else{
			printMessage(1, "Disk choosen\n");
			strcat(device, "/dev/");
			strcat(device, selected);
		}

	}while(!validDisk);
} 
int checkDiskValid(char *disk){
	char line[1024];
	char command[100]="lsblk /dev/";
	strcat(command, disk);
	strcat(command," 2>&1");
	FILE *result = run(command);
	fgets(line, sizeof(line),result); 
	pclose(result);
	if(strstr(line, "not a block device")) 
		return 0; 
	return 1;
}
void checkBIOS(){ 
	char line[1024];
	FILE *commandOut = run("ls /sys/firmware/efi/efivars 2>&1"); 
	//Only need to check first line because if there was an error it would return so.  
	fgets(line, sizeof(line),commandOut); 
	if(strstr(line, "No such")) 
		UEFI = 0; 
	else{
		UEFI = 1;
	}
	pclose(commandOut);
	if(UEFI)
        	printMessage(1, "Determined BIOS Config | UEFI: True\n");
	else
        	printMessage(1, "Determined BIOS Config | UEFI: False\n");
}
int checkNetwork(){
	char line[1024];
	FILE *result = run("ping 1.1.1.1 -c 1");
	fgets(line, sizeof(line),result);
	fgets(line, sizeof(line),result);
	pclose(result);
	if(strstr(line,"icmp_seq=") && strstr(line,"ttl=") && strstr(line, "time")){
		printMessage(1, "Network connection found\n");
		return 1; 
	}
	printMessage(3,"No network connection found\n");
	return 0;
}
void setTimezone(){
	char line[1024];
	run("timedatectl set-timezone \"$(curl -s --fail https://ipapi.co/timezone)\"");
	printMessage(1, "Succesfully set timezone\n");
}
FILE *run(char* command){
	char line[1024];
        FILE *result;
        result = popen(command, "r");
        if(result == NULL){
                printf("There is some error running command: ", command);
                exit(1);
        }
        return result;
}
void printFile(FILE *input){
	char line[1024];
        while(fgets(line, sizeof(line), input)){
                printf("%s",line);
        }
}
