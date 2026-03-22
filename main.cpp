#include "dfs.hpp"

int main() {
  try {
    auto graph = std::make_shared<Graph>(7);
    graph->add_edge(0, 1);
    graph->add_edge(0, 2);
    graph->add_edge(1, 3);
    graph->add_edge(2, 4);
    graph->add_edge(3, 5);
    graph->add_edge(4, 5);
    graph->add_edge(5, 6);
    graph->add_edge(1, 2);
    graph->add_edge(3, 4);
    std::cout << "Структура графа\n";
    for (int i = 0; i < graph->size(); ++i) {
        std::cout << i << ": ";
        for (int v : graph->neighbors(i)) {
            std::cout << v << " ";
        }
        std::cout << std::endl;
    }
    auto shared_visited = std::make_shared<std::vector<bool>>(graph->size(), false);
    Scheduler scheduler;
    scheduler.add_task("DFS_1", dfs_cooperative(graph, shared_visited, 0, "Task1"));
    scheduler.add_task("DFS_2", dfs_cooperative(graph, shared_visited, 3, "Task2"));
    scheduler.add_task("DFS_3", dfs_cooperative(graph, shared_visited, 4, "Task3"));
    std::cout << "Запуск планировщика.";
    std::cout << " Активных задач: " << scheduler.active_tasks_count() << "\n";
    scheduler.run();
  } catch (const std::exception &e) {
    std::cerr << "Критическая ошибка: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}