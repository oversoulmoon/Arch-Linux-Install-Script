#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


FILE *run(char*);
void printFile(FILE *);
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

int UEFI = 1;
char timezone[100];
char device[100];
char name[100];

#define error 0
#define success 1
#define warning 2

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

int main(){
	system("clear");
        checkBIOS();
        setTimezone();
	int has_internet = checkNetwork();
	while(!has_internet){
		printMessage(3,"Please make sure you have a network connection");
		sleep(3);
		has_internet = checkNetwork();
	}
	getDisk();
	partitionDisk();
	formatDisk();
	mountPartitions();
	installArch();
}
void installArch(){
	FILE *cpu = run("cat /proc/cpuinfo | grep \"vendor_id\" | head -n 1");
	char line[1024];
	char packages[1024]="base linux linux-firmware networkmanager base-devel git vim man-db man-pages texinfo grub efibootmgr";

	printMessage(1,"Finding CPU info");
	fgets(line, sizeof(line),cpu); 
	pclose(cpu);

	if(strstr(line,"AMD")){
		strcat(packages, " amd-ucode");
	}else if(strstr(line,"Intel")){
		strcat(packages, " intel-ucode");
	}
	printMessage(1, "Determined packages");
	snprintf(line, sizeof(line), "pacstrap -K /mnt %s", packages);
	printMessage(2, "Installing packages, this may take a while!");
	system(line);
		
	system("genfstab -U /mnt >> /mnt/etc/fstab");
	printMessage(1, "Generated mount files");

	snprintf(line, sizeof(line), "arch-chroot /mnt && ln -sf /usr/share/zoneinfo/%s /etc/localtime && hwclock --systohc", packages);
	system(line); 
	printMessage(1, "Set timezone and synchronized clock");

	snprintf(line, sizeof(line), "arch-chroot /mnt && echo \"en_US.UTF-8 UTF-8\" >> /etc/locale.gen && locale-gen && touch /etc/locale.conf && echo \"LANG=en_US.UTF-8\" >> /etc/locale.conf");
	system(line);
	printMessage(1, "Configured locales");

	snprintf(line, sizeof(line), "arch-chroot /mnt && echo \"LinuxSystem\" >> /etc/hostname");
	system(line);
	printMessage(1, "Configured locales");


	snprintf(line, sizeof(line), "arch-chroot /mnt && echo \"root:root\" | chpasswd ");
	system(line);
	printMessage(1, "Changed root password to: root");


	snprintf(line, sizeof(line), "arch-chroot /mnt && systemctl enable NetworkManager");
	system(line);
	printMessage(1, "Enabled services");
	
	if(UEFI){
		system("arch-chroot /mnt && grub-install --target=x86_64-efi --efi-directory=/mnt/boot --bootloader-id=GRUB");
	}else{
		snprintf(line, sizeof(line), "arch-chroot /mnt && grub-install --target=i386-pc %s", device);
		system(line);
	}
	system("grub-mkconfig -o /boot/grub/grub.cfg");
	printMessage(1, "Configured boot loader");

	printMessage(2, "Installation done, restarting system");
	system("reboot");
}
void mountPartitions(){
	char command[1024];
	snprintf(command, sizeof(command),"mount %s3 /mnt &> /dev/null",device);
	system(command);
	snprintf(command, sizeof(command),"swapon %s2 &> /dev/null",device);
	system(command);
	if(UEFI){
		snprintf(command, sizeof(command),"mount --mkdir %s1 /mnt/boot &> /dev/null",device);
		system(command);
	}
	printMessage(1,"Mounted disk\n");
}
void formatDisk(){
	char command[1024];
	snprintf(command, sizeof(command),"mkfs.ext4 -F %s3 &> /dev/null",device);	
	system(command);
	snprintf(command, sizeof(command),"mkswap %s2 &> /dev/null",device);	
	system(command);
	if(UEFI){
		snprintf(command, sizeof(command),"mkfs.fat -F 32 %s1 &> /dev/null",device);	
		system(command);
	}
	printMessage(1,"Formatted disk \n");
}
void partitionDisk(){
	//from here all size are in MiB
	signed int swapSize = 512;
	char answer[1025];
	snprintf(answer,sizeof(answer),"umount %s?* &> /dev/null",device);
	system(answer);
	system("swapoff -a");
	do{
		printMessage(2, "The default swap size is 512 MiB, do you wish to change this? (y/n):");
		scanf("%s",answer);
	}while(strcmp(answer,"n") != 0 && strcmp(answer,"y") != 0 );

	if(strcmp(answer, "y")== 0){
		getInteger("test",&swapSize);
	}

	if(UEFI){
		system("echo \"label: gpt\" > /tmp/partitionLayout");
		system("echo \"1: size= 614400\" >> /tmp/partitionLayout");
	}else{
		system("echo \"label:dos\" > /tmp/partitionLayout");
	}
	snprintf(answer, sizeof(answer),"echo \"2: size= %d\" >> /tmp/partitionLayout",swapSize *2048);	
	system(answer);
	system("echo \"3: size= \" >> /tmp/partitionLayout");
	snprintf(answer, sizeof(answer),"sfdisk %s < /tmp/partitionLayout &>/dev/null",device);	
	system(answer);
	printMessage(1,"Partitioned disk\n");
}
int getInteger(const char *prompt, int *i) {
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
	switch(type){
		case 0:
			printf("\[ " ANSI_COLOR_RED "ERROR" ANSI_COLOR_RESET " ] ");
			printf(message);
			break;
		case 1:
			printf("\[ " ANSI_COLOR_GREEN "DONE" ANSI_COLOR_RESET " ] ");
			printf(message);
			break;
		case 2:
			printf("\[ " ANSI_COLOR_YELLOW "WARNING" ANSI_COLOR_RESET " ] ");
			printf(message);
			break;
		default:
			printf(message);
			break;
	}

}

void getDisk(){
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
	char command[100]="lsblk /dev/";
	strcat(command, disk);
	strcat(command," 2>&1");
	char line[1024]; 
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
	pclose(commandOut);
	if(UEFI)
        	printMessage(1, "Determined BIOS Config | UEFI: True\n");
	else
        	printMessage(1, "Determined BIOS Config | UEFI: False\n");
}
int checkNetwork(){
	FILE *result = run("ping 1.1.1.1 -c 1");
	char line[1024];
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
	run("timedatectl set-timezone \"$(curl -s --fail https://ipapi.co/timezone)\"");
	printMessage(1, "Succesfully set timezone\n");
}
FILE *run(char* command){
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
