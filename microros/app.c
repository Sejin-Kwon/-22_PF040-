#include <stdio.h> // 기본 io 헤더파일
#include <unistd.h>// 유닉스 관련 헤더파일

#include <rcl/rcl.h> // ros 클라이언트 헤더파일
#include <rcl/error_handling.h> // ros 오류 헤더파일

#include <driver/gpio.h> // gpio 헤더파일
#include <driver/ledc.h> // ledc 헤더파일 esp32용 pwm제어
#include <driver/timer.h> // timer 헤더파일 

#include <esp_err.h> // esp error 파일

#include <rclc/rclc.h> // c언어를 위한 ros 클라이언트 
#include <rclc/executor.h> // ros 실행자 헤더파일

#include <geometry_msgs/msg/twist.h> // 메세지 헤더파일
#include <geometry_msgs/msg/quaternion.h> // 메세지 헤더파일
#include <nav_msgs/msg/odometry.h> // 메세지 헤더파일

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h" // freertos 헤더파일
#include "freertos/task.h" // freertos 헤더파일
#endif

#define RCCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){printf("Failed status on line %d: %d. Aborting.\n",__LINE__,(int)temp_rc);vTaskDelete(NULL);}}// ROS client 생성 확인
#define RCSOFTCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){printf("Failed status on line %d: %d. Continuing.\n",__LINE__,(int)temp_rc);}}// ROS client 생성 확인

#define PWM_L_PIN 27 // 모터L PWM핀
#define PWM_R_PIN 25 // 모터R PWM핀
#define DIR_L_PIN 14 // 모터L 방향핀
#define DIR_R_PIN 17 // 모터R 방향핀

#define ENCL_A_PIN 23 // 엔코더L A핀
#define ENCL_B_PIN 5 // 엔코더L B핀
#define ENCR_A_PIN 19 // 엔코더R A핀
#define ENCR_B_PIN 18 // 엔코더R B핀


// pub sub 생성
rcl_subscription_t velocity_sub; // 모터 velocity(linear, angular) subscriber 선언
rcl_publisher_t odom_pub; // odometry publisher 선언


// msg 생성
geometry_msgs__msg__Twist velocity_msg; // 모터 velocity msg 선언
nav_msgs__msg__Odometry odom_msg; // odometry msg 선언

geometry_msgs__msg__Quaternion quaternion; // odometry 변환용 msg 선언


//변수 생성
double target_vel_L, target_vel_R, encoder_L, encoder_R; 


//함수 선언
static void encoderL_A_isr();
static void encoderL_B_isr();
static void encoderR_A_isr();
static void encoderR_B_isr();
static bool motor_timer_isr_callback();


//gpio
static void motor_gpio_init()
{
	// gpio 설정
	gpio_config_t io_conf = {};
	io_conf.intr_type = GPIO_INTR_DISABLE; // interrupt 비활성화
	io_conf.mode = GPIO_MODE_OUTPUT; // output으로 설정
	io_conf.pin_bit_mask = ((1ULL<<DIR_L_PIN) | (1ULL<<DIR_R_PIN)); // 14, 17번 핀
	io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE; // 풀다운 활성화
	io_conf.pull_up_en = GPIO_PULLUP_DISABLE; // 풀업 비활성화
	gpio_config(&io_conf);
}


// pwm
#define LEDC_TIMER              LEDC_TIMER_0 // 0~3
#define LEDC_MODE               LEDC_HIGH_SPEED_MODE // 고속모드는 하드웨어로 노이즈 없이 듀티변경, 저속모드는 소프트웨어로 듀티변경
#define LEDC_DUTY_RES           LEDC_TIMER_11_BIT // Set duty resolution to 11 bits
#define LEDC_FREQUENCY          (20480) // 40MHz / 2^11 Frequency in Hertz.  40M / 2048

#define MOTOR_L LEDC_CHANNEL_0 // PWM 채널0~7
#define MOTOR_R LEDC_CHANNEL_1

static void ledc_init(void)
{
    // pwm 타이머 설정
	ledc_timer_config_t ledc_timer = {
		.speed_mode       = LEDC_MODE, // 고속모드는 하드웨어로 노이즈 없이 듀티변경, 저속모드는 소프트웨어로 듀티변경
		.timer_num        = LEDC_TIMER, // pwm 주기를 반복할 타이머 설정
		.duty_resolution  = LEDC_DUTY_RES, // 듀티 해상도 설정 최대 해상도는 40Mhz에 듀티해상도 1bit
		.freq_hz          = LEDC_FREQUENCY,  // pwm 주기 설정 1k = 1024
		.clk_cfg          = LEDC_AUTO_CLK // pwm 주기와 듀티해상도에 따라 자동으로 클럭 소스 설정
	};
	ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));


	// pwm 채널 설정 타이머와 gpio를 연결시킴
	ledc_channel_config_t ledc_channel0 = {
		.speed_mode     = LEDC_MODE,
		.channel        = MOTOR_L, // 채널 0~7
		.timer_sel      = LEDC_TIMER,
		.intr_type      = LEDC_INTR_DISABLE, // 카운트가 일정값에 도달하거나 fade(점진적으로 duty비 변함)가 완료되면 interrupt발생 
		.gpio_num       = PWM_L_PIN,
		.duty           = 0, // lpoint는 듀티비로 자동으로 결정. Set duty to 0%
		.hpoint         = 0 // 업카운트 모드에서 업 될 값
	};
	ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel0));

	ledc_channel_config_t ledc_channel1 = {
		.speed_mode     = LEDC_MODE,
		.channel        = MOTOR_R,
		.timer_sel      = LEDC_TIMER,
		.intr_type      = LEDC_INTR_DISABLE, // 카운트가 일정값에 도달하거나 fade(점진적으로 duty비 변함)가 완료되면 interrupt발생 
		.gpio_num       = PWM_R_PIN,
		.duty           = 0, // lpoint는 듀티비로 자동으로 결정. Set duty to 0%
		.hpoint         = 0 // 업카운트 모드에서 업 될 값
	};
	ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel1));
}


// interrupt
#define ENCL_A_GPIO GPIO_NUM_23 // 엔코더L A핀
#define ENCL_B_GPIO GPIO_NUM_5 // 엔코더L B핀
#define ENCR_A_GPIO GPIO_NUM_19 // 엔코더R A핀
#define ENCR_B_GPIO GPIO_NUM_18 // 엔코더R B핀

static void interrupt_init(void)
{
	// gpio 설정
	gpio_config_t intr_conf = {};
	intr_conf.intr_type = GPIO_INTR_ANYEDGE; // interrunpt 모든엣지
	intr_conf.mode = GPIO_MODE_INPUT; // input으로 설정
	intr_conf.pin_bit_mask = ((1ULL<<ENCL_A_GPIO) | (1ULL<<ENCL_B_GPIO) | (1ULL<<ENCR_A_GPIO) | (1ULL<<ENCR_B_GPIO)); // 5, 23, 19, 18번 핀
	intr_conf.pull_down_en = GPIO_PULLDOWN_DISABLE; // 하드에어로 풀업을 해놨기에 비활성화
	intr_conf.pull_up_en = GPIO_PULLUP_DISABLE; // 하드에어로 풀업을 해놨기에 비활성화
	gpio_config(&intr_conf);

	gpio_install_isr_service(ESP_INTR_FLAG_SHARED); // isr 우선순위 또는 종류 설정
	gpio_isr_handler_add(ENCL_A_GPIO, encoderL_A_isr, (void*) ENCL_A_GPIO); // interrupt에 ISR함수 할당 
	gpio_isr_handler_add(ENCL_B_GPIO, encoderL_B_isr, (void*) ENCL_B_GPIO);
	gpio_isr_handler_add(ENCR_A_GPIO, encoderR_A_isr, (void*) ENCR_A_GPIO);
	gpio_isr_handler_add(ENCR_B_GPIO, encoderR_B_isr, (void*) ENCR_B_GPIO);
}


// timer
#define TIMER_DIVIDER         (2) // Hardware timer clock divider
#define TIMER_SCALE           (TIMER_BASE_CLK / TIMER_DIVIDER) // convert counter value to seconds
#define ISR_TIME (0.1) // 진동 4번에 타이머 한번 작동하게 하면 이론상 0.0000001 가능하나.. 왠지 모르겠으나 안된다. 실제로 타이머가 작동하는 최소시간은 실험 상 0.00009다. 그리고 ros2 publish timer가 같이 동작하기 위한 최소시간은 실험 상 이 코드 기준 0.0002초, 안정적으로 동작하는 시간은 0.00025초다. 하지만 여기서는 엔코더 값을 잘 쌓기 위해 0.1로 한다... 시간을 더 줄이고 싶으면 엔코더값을 배열을 통해 받아서 값을 쌓으면 될 것 같다.

static void motor_timer_init(int group, int timer)
{
	// timer 설정
	timer_config_t config = {
		.divider = TIMER_DIVIDER, // 2~65536 1bit~16bit 카운트 레지스터에는 공간이 정해져 있기 때문에 주기가 작을수록 분기를 작게 잡는다. 
		.counter_dir = TIMER_COUNT_UP, // 업카운트 모드
		.counter_en = TIMER_PAUSE, // 카운터 생성 시 정지상태
		.alarm_en = TIMER_ALARM_EN, // 타이머 알람모드 활성화
		.auto_reload = TIMER_AUTORELOAD_EN, // 알람 후 카운터 값을 로드
	}; // default clock source is APB(80MHz): 클럭 소스에 따라 Hz가 다름
	timer_init(group, timer, &config); // 카운터 초기화 및 그룹 설정
	timer_set_counter_value(group, timer, 0); // 카운터 초기값 설정 및 오토리로드 시 시작 값
	timer_set_alarm_value(group, timer, ISR_TIME * TIMER_SCALE); // alarm 시간 설정
	timer_enable_intr(group, timer); // 타이머 인터럽트 설정
	timer_isr_callback_add(group, timer, motor_timer_isr_callback, NULL, ESP_INTR_FLAG_SHARED); // ISR 설정
	timer_start(group, timer); // 타이머 시작
}


//odom pub
static void odom_timer_callback(rcl_timer_t * timer, int64_t last_call_time)
{
	RCLC_UNUSED(last_call_time); 
	if (timer != NULL) 
	{
		RCSOFTCHECK(rcl_publish(&odom_pub, &odom_msg, NULL));
	}
}


// vel sub
#define WHEEL_SEPARATION 0.405 // 바퀴 간 거리 m

static void vel_subscription_callback(const void * msgin) //ros2 topic pub --once /velocity geometry_msgs/msg/Twist "{linear: {x: 0.25, y: 0.0, z: 0.0}, angular: {x: 0.0, y: 0.0, z: 0.0}}"
{
	const geometry_msgs__msg__Twist * vel_msg = (const geometry_msgs__msg__Twist *)msgin; // Twist msg로 변환 
	double linear = vel_msg->linear.x; // 선속도 저장
	double angular = vel_msg->angular.z; // 각속도 저장
	target_vel_L = linear - angular * WHEEL_SEPARATION / 2.0; // 선속도 - ((각속도*바퀴 간 거리)/2)
	target_vel_R = linear + angular * WHEEL_SEPARATION / 2.0; // 선속도 - ((각속도*바퀴 간 거리)/2)
}


// odom ISR
static void encoderL_A_isr()
{	
	if (gpio_get_level(ENCL_A_PIN) == 1) // ECNL_A가 1일 때
	{
		if (gpio_get_level(ENCL_B_PIN) == 1) // ECNL_B가 0이면 값을 더하고 아니면 뺀다. -> 모터가 시계방향인지 반시계방향으로 돌고있는 지 확인.
		{  
			encoder_L = encoder_L + 1;
		} 
		else 
		{
			encoder_L = encoder_L - 1;
		}
	}
	else                                 
	{ 
		if (gpio_get_level(ENCL_B_PIN) == 0) 
		{   
			encoder_L = encoder_L + 1;
		} 
		else 
		{
			encoder_L = encoder_L - 1;
		}
	}
}

static void encoderL_B_isr()
{
	if (gpio_get_level(ENCL_B_PIN) == 1) // ECNL_B가 1일 때
	{
		if (gpio_get_level(ENCL_A_PIN) == 0) // ECNL_A가 0이면 값을 더하고 아니면 뺀다. -> 모터가 시계방향인지 반시계방향으로 돌고있는 지 확인.
		{  
			encoder_L = encoder_L + 1;
		} 
		else 
		{
			encoder_L = encoder_L - 1;
		}
	}
	else                                 
	{ 
		if (gpio_get_level(ENCL_A_PIN) == 1) 
		{   
			encoder_L = encoder_L + 1;
		} 
		else 
		{
			encoder_L = encoder_L - 1;
		}
	}
}

static void encoderR_A_isr()
{	
	if (gpio_get_level(ENCR_A_PIN) == 1) // ECNR_A가 1일 때
	{
		if (gpio_get_level(ENCR_B_PIN) == 1) // ECNR_B가 0이면 값을 더하고 아니면 뺀다. -> 모터가 시계방향인지 반시계방향으로 돌고있는 지 확인.
		{  
			encoder_R = encoder_R + 1;
		} 
		else 
		{
			encoder_R = encoder_R - 1;
		}
	}
	else                                 
	{ 
		if (gpio_get_level(ENCR_B_PIN) == 0) 
		{   
			encoder_R = encoder_R + 1;
		} 
		else {
			encoder_R = encoder_R - 1;
		}
	}
}

static void encoderR_B_isr()
{	
	if (gpio_get_level(ENCR_B_PIN) == 1) // ECNR_B가 1일 때
	{
		if (gpio_get_level(ENCR_A_PIN) == 0) // ECNR_A가 0이면 값을 더하고 아니면 뺀다. -> 모터가 시계방향인지 반시계방향으로 돌고있는 지 확인.
		{  
			encoder_R = encoder_R + 1;
		} 
		else 
		{
			encoder_R = encoder_R - 1;
		}
	}
	else                                 
	{ 
		if (gpio_get_level(ENCR_A_PIN) == 1) 
		{   
			encoder_R = encoder_R + 1;
		} 
		else {
			encoder_R = encoder_R - 1;
		}
	}
}


// motor ISR
#define PPR 13 // 엔코더 한 바퀴당 펄스 수 
#define GEAR_RATIO 61 // 기어비

#define WHEEL_DIAMETER 0.16 // 휠 지름 m

//#define ISR_TIME 0.0000001 // 진동 4번에 타이머 한번 작동

#define _USE_MATH_DEFINES // 정확한 math 헤더파일 사용 가능
#include <math.h> // math 헤더파일 

double odom_x = 0.0; // odometry x좌표
double odom_y = 0.0; // odometry y좌표
double odom_theta = 0.00000000001; // odometry theta값

double Kp = 0.27; // 진동하기 바로 전까지 
double Ki = 5.0; // 진동하다가 멈출 때 까지
double Kd = 0.002; // 바로 잡힐 때 까지
double error_previous_L = 0.0;
double error_previous_R = 0.0;	

const double PID_MAX = 0.6338648481391764; // 정격 속도 0.275 m/s 공회전 측정 최고속도 0.63 m/s

double P_control_L, P_control_R, I_control_L, I_control_R, D_control_L, D_control_R, PID_control_L, PID_control_R; // PID 변수
double PWM_L, PWM_R; // PWM 변수

#define DUTY_MAX 2047 // duty resolution 7 bits

static bool motor_timer_isr_callback()
{	
	// 엔코더 값 가공
	double distance_L = ((encoder_L / (PPR * 4.0)) / GEAR_RATIO) * M_PI * WHEEL_DIAMETER; // 13 CPR = 한 바퀴당 13주기, 펄스 * 4체배 -> 한 바퀴 당 펄스 = 52 / 기어 비 * 바퀴 둘레 = 한 주기당 이동 거리
	double distance_R = ((encoder_R / (PPR * 4.0)) / GEAR_RATIO) * M_PI * WHEEL_DIAMETER; // 13 CPR = 한 바퀴당 13주기, 펄스 * 4체배 -> 한 바퀴 당 펄스 = 52 / 기어 비 * 바퀴 둘레 = 한 주기당 이동 거리
	
	double velocity_L =  distance_L / ISR_TIME; // 거리 / 시간 = 속도 
	double velocity_R =  distance_R / ISR_TIME; // 거리 / 시간 = 속도 
	
	
	//odometry 값 계산 및 odom publish
	double distance_sum = (distance_L + distance_R) / 2.0; // 로봇이 주기당 이동한 거리
	double theta = asin((distance_R - distance_L) / WHEEL_SEPARATION) / 2; // 호의 길이를 이용한 각도 계산
	
	if (theta > M_PI) //3.14보다 커지거나 작아지지 않게 방지
	{
		theta -= 2 * M_PI; 
	}
	else if (theta < -M_PI) 
	{
		theta += 2 * M_PI;
	}
	
	if(distance_sum != 0)
	{
		odom_x = odom_x + cos(theta) * distance_sum; // odom x좌표 계산
		odom_y = odom_y + sin(theta) * distance_sum; // odom y좌표 계산
		//odom_x = odom_x + (cos(odom_theta) * (cos(theta) * distance_sum) - sin(odom_theta) * (-sin(theta) * distance_sum)); // odom x좌표 계산
		//odom_y = odom_y + (sin(odom_theta) * (cos(theta) * distance_sum) + cos(odom_theta) * (-sin(theta) * distance_sum)); // odom y좌표 계산
	}

	if (theta != 0)
	{
		odom_theta = odom_theta + theta; // odom theta 계산
	}
	
	if (odom_theta > M_PI) //3.14보다 커지거나 작아지지 않게 방지
	{
		odom_theta -= 2 * M_PI; 
	}
	else if (odom_theta < -M_PI) 
	{
		odom_theta += 2 * M_PI;
	}
	
	double pitch = 0.0; // not use
	double roll = 0.0; // not use 
	
	double cy = cos(odom_theta * 0.5); //cos(yaw/2)
	double sy = sin(odom_theta * 0.5); //sin(yaw/2)
	double cp = cos(pitch * 0.5); //cos(pitch/2)
	double sp = sin(pitch * 0.5); //sin(pitch/2)
	double cr = cos(roll * 0.5); //cos(roll/2) 
	double sr = sin(roll * 0.5); //sin(roll/2)
	
	quaternion.x = cy * cp * sr - sy * sp * cr; // 0 
	quaternion.y = sy * cp * sr + cy * sp * cr; // 0
	quaternion.z = sy * cp * cr - cy * sp * sr; // sin(yaw/2)
	quaternion.w = cy * cp * cr + sy * sp * sr; // cos(yaw/2)
	
	odom_msg.pose.pose.position.x = odom_x;
	odom_msg.pose.pose.position.y = odom_y;
	odom_msg.pose.pose.position.z = odom_theta;
	odom_msg.pose.pose.orientation.x = quaternion.x;
	odom_msg.pose.pose.orientation.y = quaternion.y;
	odom_msg.pose.pose.orientation.z = target_vel_L;
	odom_msg.pose.pose.orientation.w = target_vel_R;
	odom_msg.twist.twist.linear.x = distance_sum / ISR_TIME;
	odom_msg.twist.twist.linear.y = distance_L / (WHEEL_DIAMETER / 2.0);
	odom_msg.twist.twist.linear.z = distance_R / (WHEEL_DIAMETER / 2.0);
	odom_msg.twist.twist.angular.x = ((distance_sum / ISR_TIME) - (WHEEL_SEPARATION / 2.0) * ((asin((distance_R - distance_L) / WHEEL_SEPARATION)) / ISR_TIME)) / (WHEEL_DIAMETER / 2.0);
	odom_msg.twist.twist.angular.y = ((distance_sum / ISR_TIME) + (WHEEL_SEPARATION / 2.0) * ((asin((distance_R - distance_L) / WHEEL_SEPARATION)) / ISR_TIME)) / (WHEEL_DIAMETER / 2.0);;
	odom_msg.twist.twist.angular.z = (asin((distance_R - distance_L) / WHEEL_SEPARATION)) / ISR_TIME;
	
	//RCSOFTCHECK(rcl_publish(&odom_pub, &odom_msg, NULL)); // odom msg publish / timer 주기가 짧아서 timer 안에 ros pub을 하면 작동이 안된다.
	
	
	// pid 제어
	double error_L = target_vel_L - velocity_L; // 오차값
	double error_R = target_vel_R - velocity_R;

	P_control_L = Kp * error_L; // 비례제어
	P_control_R = Kp * error_R;

	I_control_L = I_control_L + Ki * error_L * ISR_TIME; // 적분제어
	I_control_R = I_control_R + Ki * error_R * ISR_TIME;

	D_control_L = Kd * (error_L - error_previous_L) / ISR_TIME; // 미분제어
	D_control_R = Kd * (error_R - error_previous_R) / ISR_TIME;

	error_previous_L = error_L; // 이전 오차값
	error_previous_R = error_R;	
	
	//PID_control_R = P_control_R; // pid test
	//PID_control_L = P_control_L;
	//PID_control_R = P_control_R + I_control_R; // pid test
	//PID_control_L = P_control_L + I_control_L;
	PID_control_R = P_control_R + I_control_R + D_control_R; // output
	PID_control_L = P_control_L + I_control_L + D_control_L;	
	//PID_control_R += P_control_R + I_control_R + D_control_R; // output 누적
	//PID_control_L += P_control_L + I_control_L + D_control_L;
	
	if(PID_control_L > PID_MAX) PID_control_L = PID_MAX; // PID 최대값 및 최소값 설정
	if(PID_control_R > PID_MAX) PID_control_R = PID_MAX;
	if(PID_control_L < -PID_MAX) PID_control_L = -PID_MAX;
	if(PID_control_R < -PID_MAX) PID_control_R = -PID_MAX;
	
	if(PID_control_L > 0) PWM_L = (((PID_control_L / PID_MAX) * DUTY_MAX)); // PID값 PWM값으로 변환
	else PWM_L = (((-1 * PID_control_L / PID_MAX) * DUTY_MAX));
	if(PID_control_R > 0) PWM_R = (((PID_control_R / PID_MAX) * DUTY_MAX));
	else PWM_R = (((-1 * PID_control_R / PID_MAX) * DUTY_MAX));
	
	if(PID_control_L > 0) // PID값에 따라 방향 설정
	{
		gpio_set_level(DIR_L_PIN, 0); //0 앞 1 뒤
	}
	else if(PID_control_L < 0)
	{
		gpio_set_level(DIR_L_PIN, 1);
	}
	else
	{
		PWM_L = 0;
	}
	
	if(PID_control_R > 0) 
	{
		gpio_set_level(DIR_R_PIN, 1); //1 앞 0 뒤
	}
	else if(PID_control_R < 0)
	{
		gpio_set_level(DIR_R_PIN, 0);
	}
	else
	{
		PWM_R = 0;
	}
	
	ledc_set_duty(LEDC_MODE, MOTOR_L, PWM_L);
	ledc_set_duty(LEDC_MODE, MOTOR_R, PWM_R);
	ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, MOTOR_L)); // 모터 PWM 입력
	ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, MOTOR_R));
	
	encoder_L = 0; // 엔코더값 초기화
	encoder_R = 0; // 엔코더값 초기화
	
	return 1; // 1이면 ISR이 끝나고 양보
}


// main
void appMain(void * arg)
{
	// gpio 설정
	motor_gpio_init();
	
	
	// pwm 설정
	ledc_init();
	

	// interrupt 설정
	interrupt_init();


	// timer 설정
	motor_timer_init(TIMER_GROUP_0, TIMER_0);
	
	
        // 할당자 및 서포터 생성
	rcl_allocator_t allocator = rcl_get_default_allocator(); // 동적메모리 관리 할당자 객체 선언
	rclc_support_t support; // ROS 클라이언트 서포트 객체 선언
	RCCHECK(rclc_support_init(&support, 0, NULL, &allocator)); // 서포트 객체, 환경변수 개수, 환경변수 배열, 할당자 객체
	
	
	// node 생성
	rcl_node_t motor_node; // motor 노드 생성
	RCCHECK(rclc_node_init_default(&motor_node, "motor", "slam", &support)); // motor node 초기화


	// timer 생성 및 초기화
	rcl_timer_t odom_timer; // 주기적으로 토픽 발행 위한 타이머 객체 선언
	const unsigned int odom_timer_timeout = 1000; // 1000msec 타이머 타임아웃 상수 1초
	RCCHECK(rclc_timer_init_default(&odom_timer, &support, RCL_MS_TO_NS(odom_timer_timeout), odom_timer_callback)); // odom timer 함수 설정 및 초기화
	
	
	// pub, sub 초기화
	RCCHECK(rclc_publisher_init_default(&odom_pub, &motor_node, ROSIDL_GET_MSG_TYPE_SUPPORT(nav_msgs, msg, Odometry), "/slam/odom")); // motor node, odom pub 연결, msg 타입 설정 및 초기화
	RCCHECK(rclc_subscription_init_default(&velocity_sub, &motor_node, ROSIDL_GET_MSG_TYPE_SUPPORT(geometry_msgs, msg, Twist), "/cmd_vel")); // motor node에 velocity sub 연결, msg 타입 설정 및 초기화
		
		
	// msg 초기화
	geometry_msgs__msg__Twist__init(&velocity_msg); // 전역변수 모터 velocity msg 초기화
	nav_msgs__msg__Odometry__init(&odom_msg); // 전역변수 odometry msg 초기화
	
	
	// 실행자 생성 및 배정 및 실행
	rclc_executor_t executor; // 실행자 객체 선언
	RCCHECK(rclc_executor_init(&executor, &support.context, 2, &allocator)); // 실행자 개수 설정 및 초기화
	RCCHECK(rclc_executor_add_timer(&executor, &odom_timer)); // 실행자에 타이머 추가
	RCCHECK(rclc_executor_add_subscription(&executor, &velocity_sub, &velocity_msg, &vel_subscription_callback, ON_NEW_DATA)); // 실행자에 subscription 추가 및 subscription 함수 설정
	rclc_executor_spin(&executor); // 실행자 반복 실행
        
        
        // 스핀 종료 후 객체 완료 및 쓰레드 삭제
	RCCHECK(rclc_executor_fini(&executor)); // 실행자 완료
	geometry_msgs__msg__Twist__fini(&velocity_msg); // 전역변수 모터 velocity msg 완료
	nav_msgs__msg__Odometry__fini(&odom_msg); // 전역변수 odometry msg 완료
	RCCHECK(rcl_subscription_fini(&velocity_sub, &motor_node)); // velocity sub 완료
	RCCHECK(rcl_publisher_fini(&odom_pub, &motor_node)); // odom pub 완료
	RCCHECK(rcl_timer_fini(&odom_timer)); // odom 타이머 완료
	RCCHECK(rcl_node_fini(&motor_node));
        
  	vTaskDelete(NULL); // 쓰레드 삭제
}
