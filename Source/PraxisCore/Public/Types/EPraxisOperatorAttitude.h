#pragma once

UENUM(BlueprintType)
enum class EPraxisOperatorAttitude : uint8
{
    Resistant     UMETA(DisplayName="Resistant"),
    Compliant     UMETA(DisplayName="Compliant"),
	Engaged       UMETA(DisplayName="Engaged"),
	Proactive     UMETA(DisplayName="Proactive"),
	Passionate    UMETA(DisplayName="Passionate")
	
	/* Resistant: An employee who is reluctant to change, often challenges decisions, shows little enthusiasm
	 for tasks, and may even display negativity. 
	 Compliant: This employee meets the minimum requirements, follows rules, and completes tasks as directed,
	 but rarely goes beyond what is asked or shows independent motivation. They do what they have to do.
	 Engaged: An employee who is involved, enthusiastic about their work, and takes positive action to further the
	 organization's reputation and interests. They do what they want to do.
	 Proactive: This individual not only completes tasks well but also takes initiative to solve problems, anticipate
	 future needs, and suggest improvements without being asked. They look ahead.
	 Passionate: The highest level of attitude, this employee displays intense enthusiasm, dedication,
	 and a deep belief in their work and the company's mission. They are a powerful motivator for others and
	 consistently strive for excellence.
	 */
	
};