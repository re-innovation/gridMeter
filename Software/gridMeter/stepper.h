#ifndef Stepper_h
#define Stepper_h

#define STEPS_PER_REV 2048UL

class Stepper {
  public:
    // constructors:
    Stepper(int motor_pin_1, int motor_pin_2,
                                 int motor_pin_3, int motor_pin_4);

    // mover method:
    void step(int number_of_steps);
    int step_number;          // which step the motor is on

  private:
    void stepMotor(int this_step);

    int direction;            // Direction of rotation
    unsigned long step_delay; // delay between steps, in ms, based on speed


    // motor pin numbers:
    int motor_pin_1;
    int motor_pin_2;
    int motor_pin_3;
    int motor_pin_4;

    unsigned long last_step_time; // time stamp in us of when the last step was taken
};

#endif