#pragma once
#include "VisualWindow.h"
#include "WaterfallFlowUnit.h"
#include "ILayout.h"

using namespace Windows::Foundation;

namespace Marduk
{
    namespace Controls
    {
        ref class WaterfallFlowLayout sealed
            : ILayout
        {
        public:
            virtual RegisterReadOnlyProperty(double, _width, Width);
            virtual RegisterReadOnlyProperty(Size, Size(Width, *std::max_element(_stacks->begin(), _stacks->end())), LayoutSize);
            virtual RegisterReadOnlyProperty(int, _units->size(), UnitCount);
            RegisterReadOnlyProperty(double, _spacing, Spacing);
            RegisterReadOnlyProperty(int, _stacks->size(), StackCount);

            WaterfallFlowLayout(double spacing, double width, int stackCount);

            virtual void AddItem(int index, Platform::Object^ item, Size size);
            virtual void ChangeItem(int index, Platform::Object^ item, Size size);
            virtual void RemoveItem(int index);
            virtual void RemoveAll();

            virtual Platform::IntPtr GetVisableItems(VisualWindow window, int* firstIndex, int * lastIndex);
            virtual Rect GetItemLayoutRect(int index);
            virtual bool FillWindow(VisualWindow window);
            virtual void ChangePanelSize(double width);
            virtual Size GetItemSize(int index);

            void ChangeSpacing(double width);
            void ChangeStackCount(int stackCount);

        private:
            ~WaterfallFlowLayout();
            void Relayout();
            std::vector<WaterfallFlowUnit^>* _units;
            double _spacing;
            double _width;

            std::vector<double>* _stacks;

            int _requestRelayoutIndex = -1;
            void SetRelayoutIndex(int index);
        };
    }
}