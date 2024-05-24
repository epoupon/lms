#pragma once

#include "ScanStepBase.hpp"

namespace lms::scanner
{
	class ScanStepRecreateViews : public ScanStepBase
	{
		public:
			using ScanStepBase::ScanStepBase;

		private:
			core::LiteralString getStepName() const override { return "Recreate views"; }
			ScanStep getStep() const override { return ScanStep::RecreateViews; }
			void process(ScanContext& context) override;
	};
}
