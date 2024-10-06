#ifndef TASKCONTROLLER_H
#define TASKCONTROLLER_H

#include <stdio.h>
#include "TaskType.h"
#include "TaskBase.h"

class TaskController{

    public:

        static TaskController & instance() {
            static  TaskController * _instance = 0;
            if ( _instance == 0 ) {
                _instance = new TaskController();
            }
            return *_instance;
        }

        TaskController(){}

        void addTaskType(TaskType taskType) { taskTypes.push_back(taskType); }
        list<TaskType> getAllTaskTypes(){ return taskTypes; }

        void addTask(TaskBase *task) { tasks.push_back(task); }
        void init();

    private:
        list<TaskType> taskTypes;
        list<TaskBase*> tasks;


};

#endif