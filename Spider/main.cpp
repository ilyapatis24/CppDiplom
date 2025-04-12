#include <iostream>
#include <tuple>
#include <pqxx/pqxx>
#include <windows.h>
#include <exception>
#include <string.h>
#include <thread>
#include "ParcerINI.h"
#include "database.h"
#include "indexator.h"

#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <queue>


class safe_queue {

	std::queue<std::function<void()>> tasks;	//очередь для хранения задач
	std::mutex mutex;							//для реализации блокировки
	std::condition_variable cv;					//для уведомлений


public:
	int getTaskSize() {
		return tasks.size();
	}
	std::mutex& getMutex() {
		return mutex;
	}
	std::condition_variable& getCond() {
		return cv;
	}
	//Метод push записывает в начало очереди новую задачу.
	//При этом захватывает мьютекс, а после окончания операции нотифицируется условная переменная.
	void push(std::function<void()> task) {
		std::unique_lock<std::mutex> lock(mutex);
		tasks.push(task);
		cv.notify_one();
	}
	//Метод pop находится в ожидании, пока не придут уведомление на условную переменную.
	//При нотификации условной переменной данные считываются из очереди.
	std::function<void()> pop() {
		std::unique_lock<std::mutex> lock(mutex);
		cv.wait(lock, [this] { return !tasks.empty(); });
		auto task = tasks.front();
		tasks.pop();
		return task;
	}

};


class thread_pool {
	safe_queue task_queue;
	std::vector<std::thread> threads;
	bool stop;
public:
	thread_pool(size_t num_threads) : stop(false) {
		for (size_t i = 0; i < num_threads; ++i) {
			threads.emplace_back([this]() {
				while (task_queue.getTaskSize() > 0) {
					auto task = task_queue.pop();
					task();
				}
				});
		}
		/*for (size_t i = 0; i < num_threads; ++i) {
			threads.emplace_back(std::thread(&thread_pool::work));
		}*/
	}
	~thread_pool() {
		for (auto& thread : threads) {
			if (thread.joinable()) { thread.join(); }
		}
		stop = true;
	}
	//Метод submit помещает в очередь очередную задачу.
	//В качестве принимаемого аргумента метод может принимать или объект шаблона std::function, или объект шаблона package_task.
	template <typename Func, typename... Args>
	void submit(Func&& func, Args&&... args) {
		task_queue.push(std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
	}

	//Метод work выбирает из очереди очередную задачи и исполняет ее.
	//Данный метод передается конструктору потоков для исполнения
	void work() {
		while (task_queue.getTaskSize() > 0) {
			auto task = task_queue.pop();
			task();
		}
	}
};

// Создадим функцию для рекурсивного вызова
void recursiveMultiTreadIndexator(database& DB, int Depth, std::set<std::string> inLinkSet) {
	if (0 == Depth) return;
	// Определим количество логических процессоров
	int threads_num = std::thread::hardware_concurrency();
	//threads_num = inLinkSet.size() > 100 ? 100 : inLinkSet.size();
	std::cout << "\nNumber of treads: " << threads_num << std::endl;

	std::cout << "\n\t Current recursion level: " << Depth << std::endl;
	std::cout << "\n\t Number of new links of current level: " << inLinkSet.size() << std::endl;

	// Мьютекс для блокировки записи результатов в выходной вектор разными потоками
	std::mutex vectorMutex;

	// Создадим пул потоков
	thread_pool pool(threads_num);

	// Вектор для сбора результатов отдельных потоков
	std::vector<std::set<std::string>> outLinksVector;

	// Счётчик оставшихся для индексации ссылок
	int decrementLinks = inLinkSet.size();
	std::thread T1 = std::thread([&pool, &DB, &inLinkSet, Depth, &outLinksVector, &vectorMutex, &decrementLinks]() mutable {
		for (const auto& newLink : inLinkSet) {
			pool.submit([&DB, newLink, Depth, &outLinksVector, &pool, &vectorMutex, &decrementLinks] {
				std::cout << "\nRecursion = " << Depth << " -> ";
				std::cout << "Left links:  " << decrementLinks << " -> ";
				std::cout << "Task submitted for: " << newLink << std::endl;

				// Получим ссылки, найденные на конкретной странице
				std::set<std::string> result = indexator(DB, newLink);

				// Добавим результы в вектор, в который все потоки складывают свои данные (что с разделением ресурсов)
				// Защитим доступ к outLinksVector с помощью мьютекса
				std::lock_guard<std::mutex> lock(vectorMutex);
				outLinksVector.push_back(result);
				--decrementLinks;
				});
		}
		});
	T1.join(); // Завершения основного потока
	std::thread T2 = std::thread([&pool] {pool.work(); });
	// Подождём. когда выполнятся все потоки для очередного уровня Depth
	T2.join();

	// Сделаем из вектора выходных значений один большой set
	std::set<std::string> outLinksSet;
	for (const auto& vectorIter : outLinksVector) {
		for (const auto& setIter : vectorIter) {
			outLinksSet.insert(setIter);  // Добавляем элементы в выходной set
		}
	}
	recursiveMultiTreadIndexator(DB, Depth - 1, outLinksSet);
}

std::string DataBaseHostName;
std::string DataBaseName;
std::string DataBaseUserName;
std::string DataBasePassword;
int DataBasePort;

std::string SpiderStarPageURL;
int SpiderDepth;

std::string FinderAddress;
int FinderPort;


int main()
{
	//setlocale(LC_ALL, "Russian");
	SetConsoleCP(65001);
	SetConsoleOutputCP(65001); //UTF-8
	// Прочитаем конфигурацию в файле configuration.ini
	try {
		std::string filePath = "D:\\CppDiplom\\Spider\\configuration.ini"; // определим путь к исполняемому файлу
		std::cout << filePath;
		ParcerINI parser = ParcerINI::ParcerINI(filePath);

		DataBaseHostName = parser.get_value<std::string>("DataBase.HostName");
		DataBaseName = parser.get_value<std::string>("DataBase.DatabaseName");
		DataBaseUserName = parser.get_value<std::string>("DataBase.UserName");
		DataBasePassword = parser.get_value<std::string>("DataBase.Password");
		DataBasePort = parser.get_value<int>("DataBase.Port");

		SpiderStarPageURL = parser.get_value<std::string>("Spider.StartPageURL");
		SpiderDepth = parser.get_value<int>("Spider.Depth");

		FinderAddress = parser.get_value<std::string>("Finder.Address");
		FinderPort = parser.get_value<int>("Finder.Port");


		std::cout << "DataBaseHostName: " << DataBaseHostName << std::endl;
		std::cout << "DataBaseName: " << DataBaseName << std::endl;
		std::cout << "DataBaseUserName: " << DataBaseUserName << std::endl;
		std::cout << "DataBasePassword: " << DataBasePassword << std::endl;
		std::cout << "DataBasePort: " << DataBasePort << std::endl;
		std::cout << "SpiderStarPageURL: " << SpiderStarPageURL << std::endl;
		std::cout << "SpiderDepth: " << SpiderDepth << std::endl;
		std::cout << "FinderAddress: " << FinderAddress << std::endl;
		std::cout << "FinderPort: " << FinderPort << std::endl;
	}
	catch (const std::exception& ex) {

		std::string except = ex.what();
		std::cout << "\n" << except;
	}

	// Создадим подключение к базе данных
	database DB;

	try {
		DB.SetConnection(DataBaseHostName, DataBaseName, DataBaseUserName, DataBasePassword, DataBasePort);
		DB.table_delete();
		DB.table_create();
	}
	catch (const std::exception& ex) {
		std::cout << "Try to create tables in databse\n";
		std::string except = ex.what();
		std::cout << "\n" << except;
	}

	// Запустим индексатор для первой страницы поиска
	std::set<std::string> inLinkSet = indexator(DB, SpiderStarPageURL); SpiderDepth--;

	recursiveMultiTreadIndexator(DB, SpiderDepth, inLinkSet);

	DB.CloseConnection();
}
