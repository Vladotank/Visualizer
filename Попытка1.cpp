#include "raylib.h"
#include <vector>
#include <algorithm>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <iostream>
#include <cmath>

// стандартные переменые
const int MIN_ARRAY_SIZE = 10;
const int MAX_ARRAY_SIZE = 500;
const int DEFAULT_ARRAY_SIZE = 128; // Увеличим, чтобы TimSort был нагляднее
const int TIM_RUN = 32;             // minrun для TimSort

// цвета
const Color COLOR_BG = BLACK;
const Color COLOR_BAR_DEFAULT = BLUE;
const Color COLOR_BAR_ACTIVE = GREEN;

// класс визуализатора
class SortVisualizer {
private:
    std::vector<int> array; // где основное сохранение элементов
    int arraySize; // количество элементов

    // статистика
    std::atomic<int> comparisons; // количество сравнений
    std::atomic<int> swaps; // количество перемещений

    // управление потоком
    std::atomic<bool> isSorting; // идет сортировка?
    std::atomic<bool> shouldStop; // мгновенная остановка
    std::atomic<int> activeIndex1; // активный столбец
    std::atomic<int> activeIndex2; // сравнения
    std::atomic<int> delayMs;

    std::thread workerThread;

public:
    SortVisualizer(int size = DEFAULT_ARRAY_SIZE): // начальное значение+создание
        arraySize(size), comparisons(0), swaps(0),
        isSorting(false), shouldStop(false),
        activeIndex1(-1), activeIndex2(-1), delayMs(50) {
        GenerateArray(); // создание
    }
    // закрытие программы
    ~SortVisualizer() {
        StopSort();
    }
    void GenerateArray() { // создание нового массива
        StopSort();
        array.clear(); // старые данные в утиль
        for (int i = 0; i < arraySize; i++) {
            array.push_back(GetRandomValue(5, 300));
        }
        ResetStats(); 
    }

    void ResetStats() { //сброс статистики и подстветки
        comparisons = 0;
        swaps = 0;
        activeIndex1 = -1;
        activeIndex2 = -1;
    }

    void SetSpeed(float speedFactor) { // скорость ползунком
        // speedFactor 0.0 -> медленно (100мс), 1.0 -> быстро (0мс)
        delayMs = static_cast<int>((1.0f - speedFactor) * 50);
    }

    bool IsSorting() const {return isSorting;} // удаление кнопок

    bool CheckStop() { return shouldStop; }

    void SleepStep(){ // скорость сортировки
        if (delayMs > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
    }

    //вставки для тимсорт
    void InsertionSort(int left, int right) {
        for (int i = left + 1; i <= right; i++) { // проход 2-конец
            if (CheckStop()) return;

            int temp = array[i];
            int j = i - 1; 

            activeIndex1 = i; // текущий-зелёный
            SleepStep(); // баю бай

            while (j >= left and array[j] > temp){ // пока слева >= нынешнего
                if (CheckStop()) return;

                activeIndex1 = j + 1; //вставка
                activeIndex2 = j; //сравниваемый
                comparisons++;
                SleepStep();

                array[j + 1] = array[j]; //сдвиг вправо
                swaps++;
                j--;
            }
            array[j + 1] = temp; // темп обратно
            swaps++;
        }
    }

    // слияния для MergeSort и TimSort
    void Merge(int l, int m, int r) {
        if (CheckStop()) return;

        // временные копии
        std::vector<int> leftArr(array.begin() + l, array.begin() + m + 1);
        std::vector<int> rightArr(array.begin() + m + 1, array.begin() + r + 1);

        int i = 0, j = 0, k = l;

        while (i < leftArr.size() and j < rightArr.size()) { // в правом и левом что-то есть
            if (CheckStop()) return;

            activeIndex1 = k; // зелёным, куда запихивать
            comparisons++;
            SleepStep();
            // где меньший
            if (leftArr[i] <= rightArr[j]) {
                array[k] = leftArr[i];
                i++;
            }
            else {
                array[k] = rightArr[j];
                j++;
            }
            swaps++;
            k++;
        }
        // допихать остатки в результат
        while (i < leftArr.size()) {
            if (CheckStop()) return;
            array[k++] = leftArr[i++];
            swaps++;
            SleepStep();
        }
        while (j < rightArr.size()) {
            if (CheckStop()) return;
            array[k++] = rightArr[j++];
            swaps++;
            SleepStep();
        }
    }

    // тимсорт
    void TimSortThread() {
        int n = array.size(); // размер

        // 1) ран вставками
        for (int i = 0; i < n; i += TIM_RUN) {
            if (CheckStop()) return;
            InsertionSort(i, std::min((i + TIM_RUN - 1), (n - 1))); 
        }

        // 2) обьединение
        for (int size = TIM_RUN; size < n; size = 2 * size) {
            for (int left = 0; left < n; left += 2 * size) { // проход по соседним ран
                if (CheckStop()) return;

                int mid = left + size - 1; // середина (последний в первом)
                int right = std::min((left + 2 * size - 1), (n - 1)); // конец (последний во втором)

                if (mid < right) { // слияние, если нашлась пара
                    Merge(left, mid, right);
                }
            }
        }
        FinishSort();
    }

    // разделение на части в MergeSort
    void MergeSortRecursive(int l, int r){
        if (l < r and !CheckStop()) {
            int m =(l+r)/2; //индекс середины
            MergeSortRecursive(l, m);
            MergeSortRecursive(m + 1, r);
            Merge(l, m, r); //обьядинение половин
        }
    }
    // рекурсия всего массива
    void MergeSortThread() {
        MergeSortRecursive(0, array.size() - 1);
        FinishSort();
    }

    // QuickSort
    int Partition(int low, int high){ //опорный-самый правый, все левее
        int pivot = array[high];
        int i = (low - 1); // индекс последнего, < опорного

        for (int j = low; j <= high - 1; j++){
            if (CheckStop()) return -1;
            activeIndex1 = j; 
            activeIndex2 = high;
            comparisons++;
            SleepStep();

            if (array[j] < pivot) {
                i++;
                std::swap(array[i], array[j]); // замена текущего и гранчного
                swaps++;
            }
        }
        std::swap(array[i + 1], array[high]); //опорный после меньших
        swaps++;
        return (i + 1);
    }
    // общая рекурсия
    void QuickSortRecursive(int low, int high) {
        if (low < high and !CheckStop()) {
            int pi = Partition(low, high);
            if (pi == -1) return;
            QuickSortRecursive(low, pi - 1);
            QuickSortRecursive(pi + 1, high);
        }
    }
    //рекурсия всего
    void QuickSortThread() {
        QuickSortRecursive(0, array.size() - 1);
        FinishSort();
    }

    void FinishSort() {
        isSorting = false;
        activeIndex1 = -1; activeIndex2 = -1;
    }

    // --- Запуск ---
    void StartSort(std::string type) {
        if (isSorting) return;
        isSorting = true;
        shouldStop = false;
        ResetStats();

        if (workerThread.joinable()) workerThread.join();

        if (type == "Tim") {
            workerThread = std::thread(&SortVisualizer::TimSortThread, this);
        }
        else if (type == "Quick") {
            workerThread = std::thread(&SortVisualizer::QuickSortThread, this);
        }
        else if (type == "Merge") {
            workerThread = std::thread(&SortVisualizer::MergeSortThread, this);
        }
    }

    void StopSort(){
        if (isSorting) {
            shouldStop = true;
            if (workerThread.joinable()) workerThread.join();
            isSorting = false;
            activeIndex1 = -1; activeIndex2 = -1;
        }
    }

    // рисование
    void Draw(int screenW, int screenH){ // ш в
        float barWidth = (float)screenW / array.size(); // ширина 1 стобца
        int maxVal = 300; // макс высота

        for (int i = 0; i < array.size(); i++){
            float barHeight = ((float)array[i] / maxVal) * (screenH * 0.75f); // высота
            Color color = COLOR_BAR_DEFAULT;
            if (i == activeIndex1 or i == activeIndex2) color = COLOR_BAR_ACTIVE;

            DrawRectangleV({i * barWidth, (float)screenH - barHeight}, {barWidth, barHeight}, color); // столбец на экран
            if (barWidth > 0){ // окантовка
                DrawRectangleLines((int)(i * barWidth), (int)(screenH - barHeight), (int)barWidth, (int)barHeight, BLACK);
            }
        }

        DrawText(TextFormat("Comparisons: %d", comparisons.load()), 10, 10, 20, WHITE);
        DrawText(TextFormat("Operations: %d", swaps.load()), 10, 35, 20, WHITE);
        DrawText(TextFormat("Count: %d", arraySize), 10, 60, 20, WHITE);
    }
};

// кнопки
bool SimpleButton(Rectangle bounds, const char* text) {
    Vector2 mouse = GetMousePosition();
    bool hover = CheckCollisionPointRec(mouse, bounds); // в области
    if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) return true;
    DrawRectangleRec(bounds, hover ? DARKGRAY : GRAY);
    DrawRectangleLinesEx(bounds, 1, WHITE);
    int tw = MeasureText(text, 20);
    DrawText(text, (bounds.x + (bounds.width - tw) / 2), (bounds.y + (bounds.height - 20) / 2), 20, WHITE);
    return false;
}

int main() {
    const int screenW = 1000;
    const int screenH = 600;
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(screenW, screenH, "Sort Visualizer");
    SetTargetFPS(60);

    SortVisualizer sorter;
    float speed = 0.5f;

    while (!WindowShouldClose()){ // если не крестик
        int w = GetScreenWidth();
        int h = GetScreenHeight();

        BeginDrawing();
        ClearBackground(COLOR_BG);

        sorter.Draw(w, h);

        // интерфейс
        int btnW = 120, btnH = 40, gap = 10;
        float startX = 10, startY = 90;

        if (!sorter.IsSorting()) {
            if (SimpleButton({ startX, startY, (float)btnW, (float)btnH }, "Quick Sort")) {
                sorter.GenerateArray();
                sorter.StartSort("Quick");
            }
            if (SimpleButton({ startX + btnW + gap, startY, (float)btnW, (float)btnH }, "Merge Sort")) {
                sorter.GenerateArray();
                sorter.StartSort("Merge");
            }
            if (SimpleButton({ startX + (btnW + gap) * 2, startY, (float)btnW, (float)btnH }, "Tim Sort")) {
                sorter.GenerateArray();
                sorter.StartSort("Tim");
            }

            if (SimpleButton({ (float)w - btnW - gap, startY, (float)btnW, (float)btnH }, "New Array")) {
                sorter.GenerateArray();
            }
        }
        else {
            if (SimpleButton({ (float)w - btnW - gap, startY, (float)btnW, (float)btnH }, "STOP")) {
                sorter.StopSort();
            }
        }

        // выбор скорости
        DrawText("Speed:", startX, startY + 50, 20, GRAY);
        DrawRectangle(startX + 70, startY + 55, 200, 10, GRAY);
        DrawRectangle(startX + 70 + (int)(speed * 200), startY + 50, 10, 20, GREEN);

        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            Vector2 m = GetMousePosition();
            if ((m.x >= startX + 70) and (m.x <= startX + 270) and (m.y >= startY + 40) and (m.y <= startY + 70)){
                speed = (m.x - (startX + 70)) / 200.0f;
                if (speed < 0) speed = 0;
                if (speed > 1) speed = 1;
                sorter.SetSpeed(speed);
            }
        }
        EndDrawing();
    }
    CloseWindow();
    return 0;
}